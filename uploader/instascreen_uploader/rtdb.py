"""Thin Firebase Realtime Database REST client with optimistic concurrency.

RTDB's REST API supports compare-and-set through ETags: a GET with
``X-Firebase-ETag: true`` returns the value's ETag, and a PUT with
``if-match: <etag>`` fails with 412 if anyone changed the value in between.
``transaction`` wraps that in a retry loop, which is what keeps
``images/image_count`` consistent when two uploads race.
"""

from __future__ import annotations

import json
from collections.abc import Callable
from typing import Any, Protocol

import requests

NULL_ETAG = "NULL_ETAG"  # ETag RTDB reports for a path with no data


class ConflictError(RuntimeError):
    """A transaction kept losing the compare-and-set race."""


class Store(Protocol):
    """The subset of RTDB operations the library layer depends on."""

    def get(self, path: str, *, shallow: bool = False) -> Any: ...
    def put(self, path: str, value: Any) -> None: ...
    def delete(self, path: str) -> None: ...
    def get_with_etag(self, path: str) -> tuple[Any, str]: ...
    def put_if_match(self, path: str, value: Any, etag: str) -> tuple[bool, Any, str]: ...


class RtdbClient:
    def __init__(
        self,
        base_url: str,
        auth: str | None = None,
        *,
        session: requests.Session | None = None,
        timeout: float = 30.0,
    ) -> None:
        if not base_url:
            raise ValueError("Firebase URL is required (set FIREBASE_URL or pass --url)")
        self._base = base_url.rstrip("/")
        self._auth = auth or None
        self._http = session or requests.Session()
        self._timeout = timeout

    def _url(self, path: str) -> str:
        return f"{self._base}/{path.strip('/')}.json"

    def _params(self, **extra: str) -> dict[str, str]:
        params = dict(extra)
        if self._auth:
            params["auth"] = self._auth
        return params

    def get(self, path: str, *, shallow: bool = False) -> Any:
        params = self._params(shallow="true") if shallow else self._params()
        r = self._http.get(self._url(path), params=params, timeout=self._timeout)
        r.raise_for_status()
        return r.json()

    def put(self, path: str, value: Any) -> None:
        r = self._http.put(
            self._url(path), params=self._params(), data=json.dumps(value), timeout=self._timeout
        )
        r.raise_for_status()

    def delete(self, path: str) -> None:
        r = self._http.delete(self._url(path), params=self._params(), timeout=self._timeout)
        r.raise_for_status()

    def get_with_etag(self, path: str) -> tuple[Any, str]:
        r = self._http.get(
            self._url(path),
            params=self._params(),
            headers={"X-Firebase-ETag": "true"},
            timeout=self._timeout,
        )
        r.raise_for_status()
        return r.json(), r.headers["ETag"]

    def put_if_match(self, path: str, value: Any, etag: str) -> tuple[bool, Any, str]:
        """Conditional PUT. Returns (applied, current_value, current_etag)."""
        r = self._http.put(
            self._url(path),
            params=self._params(),
            data=json.dumps(value),
            headers={"if-match": etag},
            timeout=self._timeout,
        )
        if r.status_code == 412:  # precondition failed: body is the current value
            return False, r.json(), r.headers["ETag"]
        r.raise_for_status()
        return True, value, r.headers.get("ETag", "")


def transaction(
    store: Store, path: str, update: Callable[[Any], Any], *, max_attempts: int = 10
) -> Any:
    """Atomically apply ``update(current) -> new`` to ``path``; returns the new value."""
    current, etag = store.get_with_etag(path)
    for _ in range(max_attempts):
        new = update(current)
        applied, current, etag = store.put_if_match(path, new, etag)
        if applied:
            return new
    raise ConflictError(f"gave up on {path} after {max_attempts} conflicting writes")
