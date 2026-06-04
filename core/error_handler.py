class NovaErrorHandler:

    @staticmethod
    def handle(error, module_name="Unknown"):
        print(f"[ERROR] {module_name}: {str(error)}")

        return {
            "status": "error",
            "module": module_name,
            "message": str(error)
        }