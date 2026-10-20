import unittest
from browser_agent import validate_url, validate_timeout, classify_page


class BrowserAgentValidationTests(unittest.TestCase):
    def test_accepts_http_and_https(self):
        self.assertEqual(validate_url("https://web.whatsapp.com"), "https://web.whatsapp.com")
        self.assertEqual(validate_url("http://localhost:8080/app"), "http://localhost:8080/app")

    def test_rejects_non_web_schemes(self):
        for value in ("file:///C:/secret.txt", "javascript:alert(1)", "ms-settings:"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_url(value)

    def test_rejects_embedded_credentials_and_whitespace(self):
        for value in ("https://user:pass@example.com", "https://example.com/hello world"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_url(value)

    def test_timeout_bounds(self):
        self.assertEqual(validate_timeout(None), 30000)
        self.assertEqual(validate_timeout(5000), 5000)
        for value in (999, 120001, True, "5000"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_timeout(value)

    def test_page_classification_is_domain_agnostic(self):
        kind, required, evidence = classify_page("https://example.org/account", {
            "visible_password_input": True, "visible_auth_form": True, "auth_evidence": []
        })
        self.assertEqual(kind, "auth_required")
        self.assertTrue(required)
        self.assertIn("visible_password_input", evidence)

    def test_authenticated_content_is_not_misclassified_by_url_or_canvas(self):
        kind, required, _ = classify_page("https://example.org/login-help", {
            "visible_password_input": False, "visible_auth_form": False,
            "auth_evidence": [], "has_qr_canvas": True
        })
        self.assertEqual(kind, "content")
        self.assertFalse(required)

    def test_blank_page(self):
        kind, required, _ = classify_page("about:blank", {})
        self.assertEqual(kind, "blank")
        self.assertFalse(required)


if __name__ == "__main__":
    unittest.main(verbosity=2)
