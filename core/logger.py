import os
from datetime import datetime

class NovaLogger:

    def __init__(self):
        self.log_folder= "logs"
        self.log_file = os.path.join(self.log_folder, "history.txt")

        os.makedirs(self.log_folder, exist_ok=True)

    def write(self, command, response):
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        line = f"{timestamp} | {command} | {response}\n"

        with open(self.log_file, "a", encoding="utf-8") as file:
            file.write(line)