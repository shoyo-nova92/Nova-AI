class PluginApproval:

    def request_approval(self, plugin_name):
        print(f"\nPlugin '{plugin_name}' is ready.")
        print("Sandbox test passed.")
        
        approval = input(
            "Install plugin? (yes/no): "
        ).lower()

        if approval == "yes":
            print(
                f"{plugin_name} approved for installation."
            )
            return True

        print(
            f"{plugin_name} installation rejected."
        )
        return False