import json
from pathlib import Path


class RuntimeMetrics:

    def __init__(self):

        self.metrics_file = Path(
            "memory/runtime_metrics.json"
        )

        self.metrics_file.parent.mkdir(
            exist_ok=True
        )

        if not self.metrics_file.exists():

            with open(
                self.metrics_file,
                "w"
            ) as f:

                json.dump({}, f)

    def record_metric(

        self,

        metric_name,

        value

    ):

        with open(

            self.metrics_file,

            "r"

        ) as f:

            metrics = json.load(f)

        if metric_name not in metrics:

            metrics[metric_name] = []

        metrics[metric_name].append(
            value
        )

        with open(

            self.metrics_file,

            "w"

        ) as f:

            json.dump(

                metrics,

                f,

                indent=4

            )

    def get_average(

        self,

        metric_name

    ):

        with open(

            self.metrics_file,

            "r"

        ) as f:

            metrics = json.load(f)

        values = metrics.get(
            metric_name,
            []
        )

        if not values:

            return 0

        return sum(values) / len(values)