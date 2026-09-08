"""Table-driven routing of planned steps to deterministic or visual execution."""


class StepRouter:
    """Routes only actions that ExecutionRouter can actually dispatch as fast."""

    FAST_ACTIONS = {
        ("application", "open_app"),
        ("browser", "search"),
        ("filesystem", "create_folder"),
        ("filesystem", "create_file"),
        ("filesystem", "read_file"),
        ("filesystem", "modify_file"),
        ("filesystem", "replace_text"),
        ("filesystem", "append_file"),
        ("filesystem", "insert_at_line"),
        ("filesystem", "rollback_file"),
        ("terminal", "open_terminal"),
        ("terminal", "run_python"),
        ("terminal", "run_pytest"),
        ("terminal", "pip_install"),
        ("terminal", "build_project"),
        ("terminal", "git_status"),
        ("git", "git_add"),
        ("git", "git_commit"),
        ("git", "git_checkout"),
        ("git", "git_pull"),
        ("git", "git_push"),
    }

    def route_step(self, action):
        if not isinstance(action, dict):
            return "visual"
        route_key = (action.get("type"), action.get("action"))
        return "fast" if route_key in self.FAST_ACTIONS else "visual"
