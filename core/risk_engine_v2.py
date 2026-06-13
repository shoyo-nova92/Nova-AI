class RiskEngineV2:

    def assess(self, action):

        action = action.lower()

        low_risk = [

            "open",
            "search",
            "read",
            "view"

        ]

        high_risk = [

            "delete",
            "remove",
            "uninstall"

        ]

        critical_risk = [

            "format",
            "shutdown",
            "registry"

        ]

        for keyword in critical_risk:

            if keyword in action:

                return {

                    "risk": "CRITICAL",

                    "confidence": 0.95,

                    "approval_required": True

                }

        for keyword in high_risk:

            if keyword in action:

                return {

                    "risk": "HIGH",

                    "confidence": 0.90,

                    "approval_required": True

                }

        for keyword in low_risk:

            if keyword in action:

                return {

                    "risk": "LOW",

                    "confidence": 0.90,

                    "approval_required": False

                }

        return {

            "risk": "MEDIUM",

            "confidence": 0.50,

            "approval_required": True

        }