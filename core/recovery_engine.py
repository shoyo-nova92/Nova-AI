import os
from pathlib import Path


class RecoveryEngine:
    """
    RecoveryEngine diagnoses verification failures and constructs self-healing
    recovery actions (such as rolling back modified files from backup).
    """

    def recover(self, action, verification_result, ctx=None):
        reason = "Verification failed"
        if isinstance(verification_result, dict):
            reason = verification_result.get("reason", reason)

        target = action.get("target") if isinstance(action, dict) else None
        recovery_action = None

        if target:
            clean_target = str(target).replace("\\", "/")
            bak_file = Path(clean_target + ".bak")
            target_path = Path(clean_target)

            # If backup file exists or target is a file, rollback file
            if bak_file.exists() or (target_path.exists() and target_path.is_file()):
                recovery_action = {
                    "type": "filesystem",
                    "action": "rollback_file",
                    "action_type": "rollback_file",
                    "target": clean_target,
                }

        if not recovery_action:
            # Default retry action
            recovery_action = action

        return {
            "attempted": True,
            "recovered": True,
            "reason": reason,
            "recovery_action": recovery_action,
            "strategy": f"Rollback/retry strategy for failure: {reason}",
        }
