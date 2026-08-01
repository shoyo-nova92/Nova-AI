class RuntimeContext:
    def __init__(
        self,
        goal="",
        vision=None,
        reasoning=None,
        context=None,
        memories=None,
        raw_plan=None,
        parsed_plan=None,
        normalized_plan=None,
        expanded_plan=None,
        validated_plan=None,
        repaired_plan=None,
        executions=None,
        verification=None,
        recovery=None,
        learning=None,
        status="IDLE",
        confidence=1.0,
        metadata=None
    ):
        self.goal = goal
        self.vision = vision if vision is not None else {}
        self.reasoning = reasoning if reasoning is not None else {}
        self.context = context if context is not None else {}
        self.memories = memories if memories is not None else []
        self.raw_plan = raw_plan if raw_plan is not None else []
        self.parsed_plan = parsed_plan if parsed_plan is not None else []
        self.normalized_plan = normalized_plan if normalized_plan is not None else []
        self.expanded_plan = expanded_plan if expanded_plan is not None else []
        self.validated_plan = validated_plan if validated_plan is not None else []
        self.repaired_plan = repaired_plan if repaired_plan is not None else []
        self.executions = executions if executions is not None else []
        self.verification = verification if verification is not None else {}
        self.recovery = recovery if recovery is not None else {}
        self.learning = learning if learning is not None else {}
        self.status = status
        self.confidence = confidence
        self.metadata = metadata if metadata is not None else {}

    @property
    def plan(self):
        """Returns active plan dictionary or list representation for backward compatibility."""
        if self.repaired_plan:
            return self.repaired_plan
        return self.raw_plan

    @plan.setter
    def plan(self, value):
        if isinstance(value, dict):
            self.raw_plan = value.get("raw_plan", self.raw_plan)
            self.parsed_plan = value.get("parsed_plan", self.parsed_plan)
            self.normalized_plan = value.get("normalized_plan", self.normalized_plan)
            self.expanded_plan = value.get("expanded_plan", self.expanded_plan)
            self.validated_plan = value.get("validated_plan", self.validated_plan)
            self.repaired_plan = value.get("repaired_plan", self.repaired_plan)
            self.confidence = value.get("confidence", self.confidence)
        elif isinstance(value, list):
            self.repaired_plan = value
            if not self.raw_plan:
                self.raw_plan = value

    def to_dict(self):
        """Returns standardized output schema for Module 6."""
        is_success = (
            self.status in ("COMPLETED", "SUCCESS", "completed", "success", True)
            or self.metadata.get("completed", False)
            or self.metadata.get("success", False)
        )
        return {
            "success": is_success,
            "goal": self.goal,
            "vision": self.vision,
            "reasoning": self.reasoning,
            "context": self.context,
            "memories": self.memories,
            "raw_plan": self.raw_plan,
            "parsed_plan": self.parsed_plan,
            "normalized_plan": self.normalized_plan,
            "expanded_plan": self.expanded_plan,
            "validated_plan": self.validated_plan,
            "repaired_plan": self.repaired_plan,
            "executions": self.executions,
            "verification": self.verification,
            "recovery": self.recovery,
            "learning": self.learning,
            "status": self.status,
            "confidence": self.confidence,
            "metadata": self.metadata,
            # Backward compatibility nested structure for legacy tests
            "data": {
                "goal": self.goal,
                "context": self.context,
                "memories": self.memories,
                "plan": self.repaired_plan or self.raw_plan,
                "executions": self.executions,
                "verification": self.verification,
            }
        }
