"""Real local Chromium/DOM/download tests. No accounts, purchases or messages."""
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from playwright.sync_api import sync_playwright
import browser_agent as agent


class Fixture(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        if self.path == "/artifact":
            body = "Exact download नमस्ते\n".encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Disposition", 'attachment; filename="fixture.txt"')
        else:
            body = b'''<!doctype html><title>Local QA</title>
              <p id="result">Release evidence is available.</p>
              <input aria-label="Payload"><button onclick="document.querySelector('#result').textContent='Recorded: '+document.querySelector('input').value">Record</button>
              <a href="/artifact" download>Get artifact</a>
              <a href="/child" target="_blank">Open child</a>
              <dialog id="notice"><p>Blocking notice</p><button onclick="this.parentElement.close()">Dismiss notice</button></dialog>
              <button onclick="document.querySelector('dialog').showModal()">Show notice</button>
              <input type="password" value="never-expose-this">'''
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class BrowserWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Fixture)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.url = f"http://127.0.0.1:{cls.server.server_port}/"
        cls.playwright = sync_playwright().start()
        cls.browser = cls.playwright.chromium.launch(channel="msedge", headless=True)

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.playwright.stop()
        cls.server.shutdown()
        cls.server.server_close()

    def setUp(self):
        agent._context = self.browser.new_context(accept_downloads=True)
        agent._page = agent._context.new_page()
        agent._channel = "msedge"
        agent._session_mode = "isolated"
        agent._downloads.clear()
        agent.watch_page(agent._page)
        agent._page.goto(self.url)

    def tearDown(self):
        agent._context.close()
        agent._context = None
        agent._page = None

    def element(self, text):
        return next(e for e in agent.observe_page(agent._page)["elements"] if e["text"] == text)

    def test_unicode_input_submit_and_observed_effect(self):
        agent.execute({"command": "type", "element_id": self.element("Payload")["id"], "content": "Hello नमस्ते 😀"})
        result = agent.execute({"command": "click", "element_id": self.element("Record")["id"]})
        self.assertIn("Recorded: Hello नमस्ते 😀", result["post_action_observation"]["visible_text"])

    def test_readable_evidence_excludes_unsent_editable_content(self):
        agent._page.evaluate("() => {const composer=document.createElement('div'); composer.contentEditable='true'; composer.textContent='unsent draft only'; document.body.appendChild(composer);}")
        observed=agent.observe_page(agent._page)
        self.assertIn("unsent draft only",observed["visible_text"])
        self.assertNotIn("unsent draft only",observed["readable_text"])

    def test_download_completion_exact_bytes_no_overwrite(self):
        with tempfile.TemporaryDirectory(prefix="NovaDownloadTest_") as directory:
            result = agent.execute({"command": "download", "element_id": self.element("Get artifact")["id"], "location": directory})
            self.assertTrue(result["completed"])
            self.assertEqual(Path(result["path"]).read_bytes(), "Exact download नमस्ते\n".encode("utf-8"))
            self.assertIn("download_completed", result["effects"])
            with self.assertRaisesRegex(ValueError, "already exists"):
                agent.execute({"command": "download", "element_id": self.element("Get artifact")["id"], "location": directory})
            self.assertEqual(Path(result["path"]).read_bytes(), "Exact download नमस्ते\n".encode("utf-8"))

    def test_popup_is_observed_as_new_tab(self):
        result = agent.execute({"command": "click", "element_id": self.element("Open child")["id"]})
        self.assertIn("new_tab_opened", result["effects"])
        self.assertTrue(result["url"].endswith("/child"))

    def test_ordinary_click_cannot_start_declared_download(self):
        with self.assertRaisesRegex(ValueError,"download"):
            agent.execute({"command":"click","element_id":self.element("Get artifact")["id"]})
        self.assertFalse(agent._downloads)

    def test_dialog_link_and_sensitive_field_observations(self):
        before = agent.observe_page(agent._page)
        self.assertNotIn("never-expose-this", str(before))
        self.assertTrue(next(e for e in before["elements"] if e["text"] == "Get artifact")["href"].endswith("/artifact"))
        result = agent.execute({"command": "click", "element_id": self.element("Show notice")["id"]})
        self.assertTrue(result["post_action_observation"]["dialogs"])
        self.assertTrue(any(e["blocked"] for e in result["post_action_observation"]["elements"] if e["text"] == "Record"))

    def test_tabs_are_filtered_and_ids_select_observed_page(self):
        page = agent._context.new_page()
        page.goto(self.url + "child")
        unfiltered = agent.execute({"command": "tabs"})
        self.assertTrue(all("title" not in tab and "url" not in tab for tab in unfiltered["tabs"]))
        filtered = agent.execute({"command": "tabs", "target": "/child"})
        self.assertEqual(len(filtered["tabs"]), 1)
        selected = agent.execute({"command": "select_tab", "target": filtered["tabs"][0]["id"]})
        self.assertTrue(selected["url"].endswith("/child"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
