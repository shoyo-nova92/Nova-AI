"""Small defensive parser for JSON returned by text-only LLM calls."""

import json


def extract_json_object(raw):
    """Return the first JSON object in an LLM response, or ``None``.

    This deliberately follows the defensive fence-removal and substring fallback
    used by InputRouter, but stays schema-agnostic for other LLM callers.
    """
    if not raw:
        return None

    cleaned = str(raw).strip()
    if cleaned.startswith("```"):
        lines = [line for line in cleaned.split("\n") if not line.strip().startswith("```")]
        cleaned = "\n".join(lines).strip()

    try:
        data = json.loads(cleaned)
    except json.JSONDecodeError:
        start = cleaned.find("{")
        end = cleaned.rfind("}") + 1
        if start == -1 or end <= start:
            return None
        try:
            data = json.loads(cleaned[start:end])
        except json.JSONDecodeError:
            return None

    return data if isinstance(data, dict) else None
