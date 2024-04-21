from datetime import timedelta
from signal import Signals


class MesaCIException(Exception):
    pass


class MesaCITimeoutError(MesaCIException):
    def __init__(self, *args, timeout_duration: timedelta) -> None:
        super().__init__(*args)
        self.timeout_duration = timeout_duration


class MesaCIRetryError(MesaCIException):
    def __init__(self, *args, retry_count: int, last_job: None) -> None:
        super().__init__(*args)
        self.retry_count = retry_count
        self.last_job = last_job


class MesaCIParseException(MesaCIException):
    pass


class MesaCIKnownIssueException(MesaCIException):
    """Exception raised when the Mesa CI script finds something in the logs that
    is known to cause the LAVA job to eventually fail"""

    pass


class MesaCIKillException(MesaCIException):
    """
    The MesaCIKillException exception is raised to indicate that a MesaCI
    process has been killed or terminated. This exception may be raised when the
    MesaCI process receives one of the following signals: SIGINT, SIGTERM, or
    SIGQUIT. The purpose of this exception is to allow the MesaCI process to
    handle the signal and exit gracefully.

    Attributes:
    signal (int): The signal number that caused the script to be killed.
    """

    def __init__(self, signal_num: int) -> None:
        self.signal_num = signal_num
        self.signal_name = Signals(self.signal_num).name

        message: str = (
            f"MesaCI process terminated with signal {signal_num} ({self.signal_name})"
        )
        super().__init__(message)
