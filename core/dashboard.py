class RuntimeDashboard:

    def display(

        self,

        context,

        plan,

        memories,

        verification,

        recovery

    ):

        print(
            "\n=========================="
        )

        print(
            "NOVA CONTROL CENTER"
        )

        print(
            "=========================="
        )

        print(
            f"\nProject: "
            f"{context.get('project')}"
        )

        print(
            f"Activity: "
            f"{context.get('activity')}"
        )

        print(
            f"\nPlan Steps: "
            f"{len(plan)}"
        )

        print(
            f"Memory Hits: "
            f"{len(memories)}"
        )

        print(
            f"\nVerification: "
            f"{verification}"
        )

        print(
            f"\nRecovery: "
            f"{recovery}"
        )

        print(
            "\n==========================\n"
        )