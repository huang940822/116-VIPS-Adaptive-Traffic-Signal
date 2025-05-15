import argparse
import datetime
import enum
import glob
import logging
import os
import re

from collections import deque
from itertools import chain
from typing import Dict, List

# Regular expression to match log lines
log_start_pattern = re.compile(
    r"^\[(?P<timestamp>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]"
    r"\[(?P<level>\w+)\]"
    r"\[(?P<module>\w+)\]"
    r"\[*?(?P<file>[\w\/\.]*)\:*(?P<line>\d*)\]*\s*-*\s*"
    r"(?P<message>.*)"
)

# Set up logging configuration
logger_format = '[%(asctime)s][%(levelname)s][%(filename)s:%(lineno)d] - %(message)s'
logging.basicConfig(
        format=logger_format,
        level=logging.INFO,
        datefmt='%Y-%m-%d %H:%M:%S')
logger = logging.getLogger(__name__)

class LogLevel(enum.Enum):
    TRACE = 0
    DEBUG = 1
    INFO = 2
    WARNING = 3
    ERROR = 4
    FATAL = 5

    @classmethod
    def from_string(cls, level_str):
        try:
            return cls[level_str.upper()]
        except KeyError:
            raise ValueError(f'Invalid log level: {level_str}')

    def __str__(self):
        return self.name

def _parse_args():
    """
    Parse command line arguments.

    Returns:
        argparse.Namespace: Parsed command line arguments.
    """
    parser = argparse.ArgumentParser(description='Filter log files based on various criteria.')
    parser.add_argument('--start', type=lambda s: datetime.datetime.strptime(s, '%Y-%m-%d %H:%M:%S'), help='Start time in the format YYYY-MM-DD HH:MM:SS')
    parser.add_argument('--end', type=lambda s: datetime.datetime.strptime(s, '%Y-%m-%d %H:%M:%S'), help='End time in the format YYYY-MM-DD HH:MM:SS')
    parser.add_argument('--log-level', type=str, help='Log levels to filter (comma-separated)')
    parser.add_argument('--module-name', type=str, help='Module names to filter (comma-separated)')
    parser.add_argument('--include-content', action="append", metavar="PATTERN", help='Content to include (regex pattern), match any of them')
    parser.add_argument('--exclude-content', action="append", metavar="PATTERN", help='Content to exclude (regex pattern), match any of them')

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--log-folder', type=str, help='Path to the folder containing log files')
    group.add_argument('--log-file', type=str, help='Path to the log file')
    parser.add_argument('--output-file', type=str, required=True, help='Path to the output file')

    parser.add_argument('--debug', action='store_true', help='Enable debug output')
    parser.add_argument('--before', type=int, default=0, help='Number of log entries before the match to include')
    parser.add_argument('--after', type=int, default=0, help='Number of log entries after the match to include')

    args = parser.parse_args()
    if args.log_level:
        args.log_level = [LogLevel.from_string(level.strip()) for level in args.log_level.split(',')]
    if args.module_name:
        args.module_name = [module.strip() for module in args.module_name.split(',')]

    return args

def _setup_logger_level(debug):
    """
    Set the logging level based on the debug flag.
    Args:
        debug (bool): If True, set logging level to DEBUG, else INFO.
    """
    if debug:
        logger.setLevel(logging.DEBUG)

def _parse_log_file(filepath):
    """
    Parse the log file and yield log entries as dictionaries.
    Each log entry is a dictionary with the following keys:
    - timestamp: datetime object
    - level: LogLevel enum
    - module: str
    - file: str (optional)
    - line: int (optional)
    - message: str

    Args:
        filepath (str): Path to the log file.

    Yields:
        Dict[str, object]: A dictionary representing a log entry.

    Raises:
        FileNotFoundError: If the log file does not exist.
        ValueError: If the log file format is invalid.

    Example:
        for log_entry in parse_log_file('example.log'):
            print(log_entry)
    """
    logger.debug(f"Parsing log file: {os.path.abspath(filepath)}")
    with open(filepath, "r") as file:
        current_log = None
        while True:
            line = file.readline()
            if not line:
                if current_log:
                    if "message" not in current_log:
                        current_log = None
                        continue
                    yield current_log
                break

            match = log_start_pattern.match(line)
            if match:
                if current_log:
                    if "message" not in current_log:
                        current_log = None
                    else:
                        yield current_log

                current_log = {}
                current_timestamp = datetime.datetime.strptime(match.group("timestamp"), "%Y-%m-%d %H:%M:%S")
                current_log["timestamp"] = current_timestamp
                logger.debug(f"Parsed log entry with timestamp: {current_timestamp}")

                current_log_level = LogLevel.from_string(match.group("level"))
                current_log["level"] = current_log_level
                logger.debug(f"Parsed log entry with level: {current_log_level}")

                current_log["module"] = match.group("module")
                logger.debug(f"Parsed log entry with module: {current_log['module']}")

                current_filename = match.group("file")
                current_lineno = match.group("line")
                if current_filename and current_lineno:
                    current_log["file"] = current_filename
                    current_log["line"] = int(current_lineno)
                    logger.debug(f"Parsed log entry with file and line: {current_filename}:{current_lineno}")

                current_message = match.group("message").rstrip()
                if current_message:
                    current_log["message"] = current_message
                    logger.debug(f"Parsed log entry with message: {current_log['message']}")
                else:
                    current_log = None
                    continue
            else:
                if current_log:
                    logger.debug(f"Appending to log entry message: {current_log['message']}")
                    current_message = line.rstrip()
                    if current_message:
                        current_log["message"] += ("\n" + current_message)

def _filter_logs(log: Dict[str, object],
        start:datetime.datetime=None, end:datetime.datetime=None,
        log_level:List[LogLevel]=None,
        module_name:str=None,
        include_content_regex_list:List[str]=None,
        exclude_content_regex_list:List[str]=None) -> List[Dict[str, object]]:
    """
    Filter log entries based on various criteria.

    Args:
        log (Dict[str, object]): A dictionary representing a log entry.
        start (datetime.datetime, optional): Start time for filtering.
        end (datetime.datetime, optional): End time for filtering.
        log_level (LogLevel, optional): Log level for filtering.
        module_name (str, optional): Module name for filtering.
        include_content_regex_list (List[str], optional): List of regex patterns to include in the log message.
        exclude_content_regex_list (List[str], optional): List of regex patterns to exclude from the log message.
    Returns:
        bool: True if the log entry matches the filter criteria, False otherwise.
    """
    if log["timestamp"] < start:
        logger.debug(f"Skipping log entry before start time: {log['timestamp']}")
        return False
    if end and log["timestamp"] > end:
        logger.debug(f"Skipping log entry after end time: {log['timestamp']}")
        return False
    if log_level and log["level"] not in log_level:
        logger.debug(f"Skipping log entry with level {log['level']} lower than {log_level}")
        return False
    if module_name and log['module'] not in module_name:
        logger.debug(f"Skipping log entry with module {log['module']} not matching {module_name}")
        return False
    if include_content_regex_list:
        for content_regex in include_content_regex_list:
            logger.debug(f"Checking log entry with message: {log['message']}")
            logger.debug(f"Checking against regex: {content_regex}")
            content_search = re.search(content_regex, log["message"])
            if not content_search:
                logger.debug(f"Skipping log entry with message not matching regex: {log['message']}")
                return False
    if exclude_content_regex_list:
        for content_regex in exclude_content_regex_list:
            logger.debug(f"Checking log entry with message: {log['message']}")
            logger.debug(f"Checking against regex: {content_regex}")
            content_search = re.search(content_regex, log["message"])
            if content_search:
                logger.debug(f"Skipping log entry with message matching regex: {log['message']}")
                return False

    return True

def find_with_context(lst, predicate, n_before=0, m_after=0):
    """
    Find elements in a list that match a predicate and return them with context.
    Args:
        lst (List): The list to search.
        predicate (callable): A function that takes an element and returns True if it matches the criteria.
        n_before (int): Number of elements before the match to include.
        m_after (int): Number of elements after the match to include.
    Returns:
        List: A list of elements that match the predicate, including context.
    """
    idx = [i for i, v in enumerate(lst) if predicate(v)]
    spans = [range(max(0, i - n_before), min(len(lst), i + m_after + 1)) for i in idx]
    merged = sorted(set(chain.from_iterable(spans)))
    return [lst[i] for i in merged]

def _save_to_file(logs: Dict[str, object], output_file):
    """
    Save filtered logs to a file.
    Args:
        logs (List[Dict[str, object]]): List of filtered log entries.
        output_file (str): Path to the output file.
    """
    with open(output_file, "w") as file:
        for log in logs:
            if 'file' in log and 'line' in log:
                file.write(f"[{log['timestamp']}][{log['level']}][{log['module']}][{log['file']}:{log['line']}] - {log['message']}\n")
            else:
                file.write(f"[{log['timestamp']}][{log['level']}][{log['module']}]{log['message']}\n")


def main():
    args = _parse_args()
    _setup_logger_level(args.debug)

    if args.start and args.end and args.start > args.end:
        logger.error("Start time must be before end time.")
        return

    if args.include_content:
        logger.info(f"Include content regex patterns: {args.include_content}")
        for content_regex in args.include_content:
            try:
                re.compile(content_regex)
            except re.error:
                logger.error(f"Invalid regex pattern: {content_regex}")
                return
    if args.exclude_content:
        logger.info(f"Exclude content regex patterns: {args.exclude_content}")
        for content_regex in args.exclude_content:
            try:
                re.compile(content_regex)
            except re.error:
                logger.error(f"Invalid regex pattern: {content_regex}")
                return

    log_file_paths = []
    if args.log_folder:
        log_file_paths = glob.glob(f"{args.log_folder}/*.log")
        if not log_file_paths:
            logger.warning(f"No log files found in {args.log_folder}")
    elif args.log_file:
        log_file_paths.append(args.log_file)

    logs = []
    for log_file_path in log_file_paths:
        logger.info(f"Processing log file: {log_file_path}")
        buffer = [log for log in _parse_log_file(log_file_path)]
        if buffer:
            logs += buffer
            logger.info(f"Parsed {len(buffer)} log entries from {log_file_path}")
        else:
            logger.warning(f"No valid log entries found in {log_file_path}")
    if logs:
        logs = find_with_context(
            logs,
            lambda log: _filter_logs(
                log,
                start=args.start,
                end=args.end,
                log_level=args.log_level,
                module_name=args.module_name,
                include_content_regex_list=args.include_content,
                exclude_content_regex_list=args.exclude_content),
            n_before=args.before,
            m_after=args.after)
        _save_to_file(logs, args.output_file)
        logger.info(f"Filtered logs saved to {args.output_file}")
    else:
        logger.warning("No logs to save.")

if __name__ == '__main__':
    main()
