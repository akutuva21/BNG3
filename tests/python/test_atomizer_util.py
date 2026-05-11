from pytest import raises
from bionetgen.atomizer.utils.util import get_item


def test_get_item():
    # Test dictionary with existing key
    d = {"a": 1, "b": 2}
    assert get_item(d, "a") == 1
    assert get_item(d, "b") == 2

    # Test dictionary with missing key (should return None via get())
    assert get_item(d, "c") is None

    # Test list with valid index
    l = [10, 20, 30]
    assert get_item(l, 0) == 10
    assert get_item(l, 2) == 30
    assert get_item(l, -1) == 30

    # Test list with invalid index (should raise IndexError)
    with raises(IndexError):
        get_item(l, 3)

    with raises(IndexError):
        get_item(l, -4)

    # Test tuple with valid index
    t = (100, 200)
    assert get_item(t, 0) == 100

    # Test tuple with invalid index
    with raises(IndexError):
        get_item(t, 2)
