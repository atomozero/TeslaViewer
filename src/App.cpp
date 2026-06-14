#include <Application.h>

#include "MainWindow.h"

#include <cstdio>
#include <cstring>


class TeslaApp : public BApplication {
public:
	TeslaApp(const char* root)
		: BApplication("application/x-vnd.tesla-sentry-viewer")
	{
		fWindow = new MainWindow(root);
		fWindow->Show();
	}

private:
	MainWindow* fWindow;
};


int
main(int argc, char* argv[])
{
	const char* root = "/boot/home/Desktop/Tesla";
	if (argc > 1)
		root = argv[1];

	TeslaApp app(root);
	app.Run();
	return 0;
}
