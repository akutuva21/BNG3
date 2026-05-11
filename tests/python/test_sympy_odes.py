import pytest
from unittest.mock import patch
from bionetgen.modelapi.sympy_odes import _safe_rmtree


def test_safe_rmtree_exception():
    with patch("shutil.rmtree") as mock_rmtree:
        mock_rmtree.side_effect = Exception("Mock exception")
        # Should not raise an exception
        try:
            _safe_rmtree("dummy_path")
        except Exception as e:
            pytest.fail(f"_safe_rmtree raised an exception unexpectedly: {e}")
