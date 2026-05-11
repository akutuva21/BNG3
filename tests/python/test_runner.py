import os
import pytest
from unittest.mock import patch, MagicMock, ANY
from bionetgen.modelapi.runner import run


@patch("bionetgen.modelapi.runner.BNGCLI")
def test_runner_with_out(mock_bngcli):
    mock_cli_instance = MagicMock()
    mock_bngcli.return_value = mock_cli_instance
    mock_cli_instance.result = "mock_result"

    inp = "test.bngl"
    out = "test_out"

    result = run(inp, out=out, suppress=True, timeout=10)

    mock_bngcli.assert_called_once_with(inp, out, ANY, suppress=True, timeout=10)
    mock_cli_instance.run.assert_called_once()
    assert result == "mock_result"


@patch("bionetgen.modelapi.runner.BNGCLI")
@patch("bionetgen.modelapi.runner.TemporaryDirectory")
def test_runner_without_out(mock_tempdir, mock_bngcli):
    mock_cli_instance = MagicMock()
    mock_bngcli.return_value = mock_cli_instance
    mock_cli_instance.result = "mock_result"

    mock_tempdir_instance = MagicMock()
    mock_tempdir.return_value.__enter__.return_value = "temp_out"

    inp = "test.bngl"

    result = run(inp, suppress=False, timeout=None)

    mock_tempdir.assert_called_once()
    mock_bngcli.assert_called_once_with(
        inp, "temp_out", ANY, suppress=False, timeout=None
    )
    mock_cli_instance.run.assert_called_once()
    assert result == "mock_result"


@patch("bionetgen.modelapi.runner.BNGCLI")
def test_runner_exception(mock_bngcli):
    mock_cli_instance = MagicMock()
    mock_bngcli.return_value = mock_cli_instance
    mock_cli_instance.run.side_effect = Exception("Test Exception")

    inp = "test.bngl"
    out = "test_out"

    cur_dir = os.getcwd()

    with pytest.raises(Exception, match="Test Exception"):
        run(inp, out=out)

    assert os.getcwd() == cur_dir
