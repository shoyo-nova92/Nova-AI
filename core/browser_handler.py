import webbrowser


class BrowserHandler:

    def open_website(

        self,

        url

    ):

        try:

            webbrowser.open(
                url
            )

            return {

                "success": True,

                "action":
                    f"open {url}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def search_query(

        self,

        query

    ):

        try:

            url = (

                "https://www.google.com/search?q="
                + query.replace(
                    " ",
                    "+"
                )

            )

            webbrowser.open(
                url
            )

            return {

                "success": True,

                "action":
                    f"search {query}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def open_youtube(self):

        return self.open_website(
            "https://youtube.com"
        )