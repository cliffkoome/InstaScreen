import json

import responses
from responses import matchers

from instascreen_uploader.rtdb import RtdbClient, transaction

BASE = "https://demo-default-rtdb.firebaseio.com"


@responses.activate
def test_auth_is_sent_as_query_param_and_trailing_slash_tolerated():
    responses.get(
        f"{BASE}/images/image_count.json",
        match=[matchers.query_param_matcher({"auth": "s3cret"})],
        json=3,
    )
    assert RtdbClient(BASE + "/", "s3cret").get("images/image_count") == 3


@responses.activate
def test_transaction_retries_on_412_with_server_value():
    url = f"{BASE}/images/image_count.json"
    responses.get(
        url,
        json=1,
        headers={"ETag": "a"},
        match=[matchers.header_matcher({"X-Firebase-ETag": "true"})],
    )
    # First CAS loses: server now holds 5 with etag "b".
    responses.put(
        url,
        status=412,
        json=5,
        headers={"ETag": "b"},
        match=[matchers.header_matcher({"if-match": "a"})],
    )
    responses.put(
        url, json=6, headers={"ETag": "c"}, match=[matchers.header_matcher({"if-match": "b"})]
    )

    assert transaction(RtdbClient(BASE), "images/image_count", lambda n: n + 1) == 6
    assert json.loads(responses.calls[-1].request.body) == 6
