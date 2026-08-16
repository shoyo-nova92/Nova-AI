import random
from typing import Dict, List


class ConversationHandler:
    """Handles conversational greetings and small-talk with offline quick replies.

    Zero LLM dependency. Selects from curated short 1-2 line natural responses.
    """

    SALUTATION_KEYWORDS: tuple = (
        "hey", "hi", "hello", "heya", "hola", "howdy", "greetings",
        "what's up", "sup", "yo", "hiya", "hey there", "hi there",
        "hello there", "how are you", "how you doing", "how are u",
        "how are you doing", "how are things", "good morning",
        "good afternoon", "good evening", "hey nova", "hi nova",
        "hello nova", "hey x", "hi x", "hello x",
    )

    EXIT_KEYWORDS: tuple = (
        "exit", "quit", "bye", "goodbye", "good bye", "see ya",
        "see you", "cya", "later", "shutdown", "terminate", "close nova",
    )

    THANK_YOU_KEYWORDS: tuple = (
        "thanks", "thank you", "thx", "ty", "appreciate it", "much appreciated",
    )

    GREETING_RESPONSES: List[str] = [
        "Hey! How are you doing? How may I help you?",
        "Yeah? What do you need?",
        "I'm here. What is it this time?",
        "Hey there, ready when you are.",
        "All good on my end. How can I assist?",
        "Hi! What's on your mind today?",
        "Yo, what's up?",
        "Howdy! What can I do for you?",
        "Hey hey. Need a hand with something?",
    ]

    HOW_ARE_YOU_RESPONSES: List[str] = [
        "I'm doing great, thanks for asking! How about you?",
        "All systems operational. What do you need?",
        "Feeling productive today. Let's get to it.",
        "Running smoothly. How can I help?",
    ]

    THANK_YOU_RESPONSES: List[str] = [
        "No problem! Anything else?",
        "You're welcome. Happy to help.",
        "Anytime! What's next?",
        "Of course. Just say the word.",
    ]

    EXIT_RESPONSES: List[str] = [
        "Goodbye! See you soon.",
        "Later! Take care.",
        "Bye for now!",
        "Shutting down. Catch you next time.",
    ]

    DEFAULT_RESPONSES: List[str] = [
        "I'm listening. Please provide a command or a goal.",
        "Ready for your next instruction.",
        "Yes? What can I do for you?",
    ]

    @classmethod
    def is_conversational(cls, command: str) -> bool:
        """Detect if the command is a conversational greeting or salutation."""
        if not command:
            return False
        normalized = command.strip().lower()
        if not normalized:
            return False
        return any(keyword in normalized for keyword in cls.SALUTATION_KEYWORDS)

    @classmethod
    def is_exit(cls, command: str) -> bool:
        """Detect if the command is an exit/shutdown command."""
        if not command:
            return False
        normalized = command.strip().lower()
        if not normalized:
            return False
        return any(keyword in normalized for keyword in cls.EXIT_KEYWORDS)

    @classmethod
    def is_thank_you(cls, command: str) -> bool:
        """Detect if the command is a thank-you."""
        if not command:
            return False
        normalized = command.strip().lower()
        if not normalized:
            return False
        return any(keyword in normalized for keyword in cls.THANK_YOU_KEYWORDS)

    @classmethod
    def respond(cls, command: str) -> Dict:
        """Select an appropriate offline quick-reply for the command."""
        normalized = (command or "").strip().lower()

        if cls.is_exit(normalized):
            response = random.choice(cls.EXIT_RESPONSES)
            return {
                "branch": "conversation",
                "category": "exit",
                "status": "handled",
                "response": response,
                "action": "exit",
            }

        if cls.is_thank_you(normalized):
            response = random.choice(cls.THANK_YOU_RESPONSES)
            return {
                "branch": "conversation",
                "category": "thanks",
                "status": "handled",
                "response": response,
            }

        if "how are you" in normalized or "how are u" in normalized or "how you doing" in normalized:
            response = random.choice(cls.HOW_ARE_YOU_RESPONSES)
            return {
                "branch": "conversation",
                "category": "greeting",
                "status": "handled",
                "response": response,
            }

        if cls.is_conversational(normalized):
            response = random.choice(cls.GREETING_RESPONSES)
            return {
                "branch": "conversation",
                "category": "greeting",
                "status": "handled",
                "response": response,
            }

        response = random.choice(cls.DEFAULT_RESPONSES)
        return {
            "branch": "conversation",
            "category": "default",
            "status": "handled",
            "response": response,
        }
