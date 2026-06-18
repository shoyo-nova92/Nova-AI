class ResultBuilder:

    @staticmethod
    def build(

        success,

        action=None,

        reason=None

    ):

        return {

            "success": success,

            "action": action,

            "reason": reason,

            "confidence": None,

            "verification": None,

            "duration": None

        }