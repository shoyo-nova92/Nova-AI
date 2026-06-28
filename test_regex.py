# test_regex.py

import re

tests = [

    "Add execution latency tracking in executor/core.ts",

    "Add schema validation to context_fusion/input.ts",

    "Create workflow_validator.py",

]

pattern = r'[\w\-/]+\.(py|ts|js|json)'

for t in tests:

    match = re.search(pattern, t)

    print()
    print(t)

    if match:
        print("FOUND:", match.group(0))
    else:
        print("NO MATCH")