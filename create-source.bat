@echo off
echo Creating Pistachio source files...

mkdir src\domain src\ports src\core src\adapters\ui src\adapters\loaders src\adapters\exporters src\adapters\rendering 2>nul

echo Created directory structure!
echo.
echo Now copy the C++ files from Claude's artifact into these folders:
echo   - src/domain/     : Model.h, Model.cpp, Geometry.h, Geometry.cpp
echo   - src/ports/      : IUIPort.h, IFileLoaderPort.h, IExporterPort.h, IRendererPort.h
echo   - src/core/       : Application.h, Application.cpp
echo   - src/adapters/ui/: ImGuiAdapter.h, ImGuiAdapter.cpp
echo   - src/adapters/loaders/: StepFileLoader.h, StepFileLoader.cpp
echo   - src/adapters/exporters/: ObjExporter.h/.cpp, StlExporter.h/.cpp
echo   - src/adapters/rendering/: OcctRenderer.h/.cpp
echo   - src/            : main.cpp
echo.
pause

 