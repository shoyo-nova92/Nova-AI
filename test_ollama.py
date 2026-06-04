import requests

response = requests.post(
    "http://127.0.0.1:11434/api/chat",
    json={
        "model": "qwen3:32b",
        "messages": [
            {
                "role": "user",
                "content": "say hello"
            }
        ],
        "stream": False
    }
)

print(response.status_code)
print(response.json())