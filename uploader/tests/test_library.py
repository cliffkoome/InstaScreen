import pytest
from conftest import make_image

from instascreen_uploader import library
from instascreen_uploader.encode import decode_node, encode, prepare_jpeg
from instascreen_uploader.rtdb import ConflictError, transaction


@pytest.fixture
def image():
    return encode(prepare_jpeg(make_image()), 1024)


def test_append_reserves_sequential_slots(store, image):
    assert library.append(store, image) == 1
    assert library.append(store, image) == 2
    assert library.image_count(store) == 2
    assert decode_node(store.get("images/image_2")) == image.jpeg


def test_append_reserves_count_before_writing_image(store, image):
    library.append(store, image)
    assert store.writes[0] == ("put", "images/image_count")
    assert store.writes[1] == ("put", "images/image_1")


def test_concurrent_writer_gets_a_different_slot(store, image):
    library.append(store, image)  # count = 1
    store.conflicts_to_inject = 1  # someone else appends between our read and write
    assert library.append(store, image) == 3
    assert library.image_count(store) == 3


def test_transaction_gives_up_under_sustained_contention(store):
    store.conflicts_to_inject = 100
    with pytest.raises(ConflictError):
        transaction(store, "images/image_count", lambda n: (n or 0) + 1, max_attempts=3)


def test_replace_extends_count(store, image):
    library.replace(store, 4, image)
    assert library.image_count(store) == 4
    library.replace(store, 2, image)
    assert library.image_count(store) == 4


def test_remove_compacts_playlist(store):
    imgs = [encode(prepare_jpeg(make_image(size=(320 + i, 240))), 1024) for i in range(3)]
    for im in imgs:
        library.append(store, im)
    library.remove(store, 1)
    assert library.image_count(store) == 2
    assert decode_node(store.get("images/image_1")) == imgs[2].jpeg  # last moved into hole
    assert decode_node(store.get("images/image_2")) == imgs[1].jpeg
    assert store.get("images/image_3") is None


def test_remove_rejects_out_of_range(store, image):
    library.append(store, image)
    with pytest.raises(ValueError):
        library.remove(store, 2)


def test_describe_and_migrate_legacy_slots(store, image):
    hexs = image.jpeg.hex().upper()
    legacy = [hexs[i : i + 2048] for i in range(0, len(hexs), 2048)]
    store.put("images/image_1", {"chunk_count": len(legacy), "chunks": legacy})
    store.put("images/image_count", 2)
    store.put("images/image_2", image.to_node())

    infos = library.describe(store)
    assert [i.version for i in infos] == [1, 2]

    assert library.migrate(store, 4096) == [1]
    node = store.get("images/image_1")
    assert node["meta"]["v"] == 2 and "chunk_count" not in node
    assert decode_node(node) == image.jpeg
