import subprocess
from unittest.mock import MagicMock, patch


def test_bngexec_success():
    from bionetgen.core.utils.utils import test_bngexec

    with patch("bionetgen.core.utils.utils.run_command") as mock_run_command:
        # Mock successful run where return code is 0
        mock_run_command.return_value = (0, "output")

        result = test_bngexec("path/to/BNG2.pl")

        assert result is True
        mock_run_command.assert_called_once_with(["perl", "path/to/BNG2.pl"])


def test_bngexec_failure():
    from bionetgen.core.utils.utils import test_bngexec

    with patch("bionetgen.core.utils.utils.run_command") as mock_run_command:
        # Mock failed run where return code is non-zero
        mock_run_command.return_value = (1, "error")

        result = test_bngexec("path/to/BNG2.pl")

        assert result is False
        mock_run_command.assert_called_once_with(["perl", "path/to/BNG2.pl"])


def test_run_command_timeout_suppress():
    from bionetgen.core.utils.utils import run_command

    with patch("bionetgen.core.utils.utils.subprocess.run") as mock_run:
        mock_rc = MagicMock()
        mock_rc.returncode = 0
        mock_run.return_value = mock_rc

        command = ["ls", "-l"]
        rc, out = run_command(command, suppress=True, timeout=10)

        assert rc == 0
        assert out == mock_rc
        mock_run.assert_called_once_with(
            command,
            timeout=10,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=None,
        )


def test_run_command_timeout_no_suppress():
    from bionetgen.core.utils.utils import run_command

    with patch("bionetgen.core.utils.utils.subprocess.run") as mock_run:
        mock_rc = MagicMock()
        mock_rc.returncode = 0
        mock_run.return_value = mock_rc

        command = ["ls", "-l"]
        rc, out = run_command(command, suppress=False, timeout=10)

        assert rc == 0
        assert out == mock_rc
        mock_run.assert_called_once_with(
            command, timeout=10, capture_output=True, cwd=None
        )


def test_run_command_no_timeout_suppress():
    from bionetgen.core.utils.utils import run_command

    with patch("bionetgen.core.utils.utils.subprocess.Popen") as mock_popen:
        mock_process = MagicMock()
        mock_process.wait.return_value = 0
        mock_popen.return_value = mock_process

        command = ["ls", "-l"]
        rc, out = run_command(command, suppress=True, timeout=None)

        assert rc == 0
        assert out == mock_process
        mock_popen.assert_called_once_with(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            bufsize=-1,
            cwd=None,
        )


def test_run_command_no_timeout_no_suppress():
    from bionetgen.core.utils.utils import run_command

    with patch("bionetgen.core.utils.utils.subprocess.Popen") as mock_popen:
        mock_process = MagicMock()
        mock_process.wait.return_value = 0
        mock_process.poll.side_effect = [None, None, None, None, 0]
        mock_process.stdout.readline.side_effect = [
            "line1\n",
            "line2\n",
            "",
            "",
            "",
            "",
            "",
        ]
        mock_popen.return_value = mock_process

        command = ["ls", "-l"]
        rc, out = run_command(command, suppress=False, timeout=None)

        assert rc == 0
        assert out == ["line1", "line2"]
        mock_popen.assert_called_once_with(
            command, stdout=subprocess.PIPE, encoding="utf8", cwd=None
        )
