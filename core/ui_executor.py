import pyautogui


class UIExecutor:

    def click_element(self, elements, target_text):
        target_text = target_text.lower()

        for element in elements:
            text = element["text"].lower()

            if target_text in text:
                bbox = element["position"]

                x = int(
                    (
                        bbox[0][0] +
                        bbox[2][0]
                    ) / 2
                )

                y = int(
                    (
                        bbox[0][1] +
                        bbox[2][1]
                    ) / 2
                )

                print(f"Clicking {text} at ({x}, {y})")

                pyautogui.moveTo(
                    x,
                    y,
                    duration=0.5
                )

                pyautogui.click()

                return True

        print("Element not found.")
        return False