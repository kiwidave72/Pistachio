# Pistachio - Create Empty File Structure
# This creates all files with headers so you know what to paste where
# Run: powershell -ExecutionPolicy Bypass .\create-files.ps1

Write-Host "Creating Pistachio file structure..." -ForegroundColor Cyan
Write-Host ""

# Create directories
$dirs = @(
    "src\domain",
    "src\ports",
    "src\core",
    "src\adapters\ui",
    "src\adapters\loaders",
    "src\adapters\exporters",
    "src\adapters\rendering"
)

foreach ($dir in $dirs) {
    if (!(Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Write-Host "Created directory: $dir" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "Creating files with placeholders..." -ForegroundColor Cyan
Write-Host ""

# Function to create file with header
function New-SourceFile {
    param(
        [string]$Path,
        [string]$Description
    )
    
    $header = @"
// ============================================================================
// $Description
// PASTE CONTENT HERE FROM CLAUDE'S INSTRUCTIONS
// ============================================================================

"@
    
    Set-Content -Path $Path -Value $header -Encoding UTF8
    Write-Host "Created: $Path" -ForegroundColor Gray
}

# Domain Layer
New-SourceFile "src\domain\Model.h" "Domain - Model Header"
New-SourceFile "src\domain\Model.cpp" "Domain - Model Implementation"
New-SourceFile "src\domain\Geometry.h" "Domain - Geometry Header"
New-SourceFile "src\domain\Geometry.cpp" "Domain - Geometry Implementation"

# Ports (Interfaces)
New-SourceFile "src\ports\IUIPort.h" "Port - UI Interface"
New-SourceFile "src\ports\IFileLoaderPort.h" "Port - File Loader Interface"
New-SourceFile "src\ports\IExporterPort.h" "Port - Exporter Interface"
New-SourceFile "src\ports\IRendererPort.h" "Port - Renderer Interface"

# Core Application
New-SourceFile "src\core\Application.h" "Core - Application Header"
New-SourceFile "src\core\Application.cpp" "Core - Application Implementation"

# Adapters - UI
New-SourceFile "src\adapters\ui\ImGuiAdapter.h" "Adapter - ImGui UI Header"
New-SourceFile "src\adapters\ui\ImGuiAdapter.cpp" "Adapter - ImGui UI Implementation"

# Adapters - Loaders
New-SourceFile "src\adapters\loaders\StepFileLoader.h" "Adapter - STEP Loader Header"
New-SourceFile "src\adapters\loaders\StepFileLoader.cpp" "Adapter - STEP Loader Implementation"

# Adapters - Exporters
New-SourceFile "src\adapters\exporters\ObjExporter.h" "Adapter - OBJ Exporter Header"
New-SourceFile "src\adapters\exporters\ObjExporter.cpp" "Adapter - OBJ Exporter Implementation"
New-SourceFile "src\adapters\exporters\StlExporter.h" "Adapter - STL Exporter Header"
New-SourceFile "src\adapters\exporters\StlExporter.cpp" "Adapter - STL Exporter Implementation"

# Adapters - Rendering
New-SourceFile "src\adapters\rendering\OcctRenderer.h" "Adapter - OpenCASCADE Renderer Header"
New-SourceFile "src\adapters\rendering\OcctRenderer.cpp" "Adapter - OpenCASCADE Renderer Implementation"

# Main entry point
New-SourceFile "src\main.cpp" "Main Entry Point"

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "All files created successfully!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "File list with descriptions:" -ForegroundColor Yellow
Write-Host ""
Write-Host "DOMAIN LAYER:" -ForegroundColor Cyan
Write-Host "  src\domain\Model.h          - Model class header"
Write-Host "  src\domain\Model.cpp        - Model class implementation"
Write-Host "  src\domain\Geometry.h       - Geometry class header"
Write-Host "  src\domain\Geometry.cpp     - Geometry class implementation"
Write-Host ""
Write-Host "PORTS (INTERFACES):" -ForegroundColor Cyan
Write-Host "  src\ports\IUIPort.h         - UI interface"
Write-Host "  src\ports\IFileLoaderPort.h - File loader interface"
Write-Host "  src\ports\IExporterPort.h   - Exporter interface"
Write-Host "  src\ports\IRendererPort.h   - Renderer interface"
Write-Host ""
Write-Host "CORE:" -ForegroundColor Cyan
Write-Host "  src\core\Application.h      - Application orchestrator header"
Write-Host "  src\core\Application.cpp    - Application orchestrator implementation"
Write-Host ""
Write-Host "ADAPTERS - UI:" -ForegroundColor Cyan
Write-Host "  src\adapters\ui\ImGuiAdapter.h   - ImGui UI header"
Write-Host "  src\adapters\ui\ImGuiAdapter.cpp - ImGui UI implementation"
Write-Host ""
Write-Host "ADAPTERS - LOADERS:" -ForegroundColor Cyan
Write-Host "  src\adapters\loaders\StepFileLoader.h   - STEP loader header"
Write-Host "  src\adapters\loaders\StepFileLoader.cpp - STEP loader implementation"
Write-Host ""
Write-Host "ADAPTERS - EXPORTERS:" -ForegroundColor Cyan
Write-Host "  src\adapters\exporters\ObjExporter.h    - OBJ exporter header"
Write-Host "  src\adapters\exporters\ObjExporter.cpp  - OBJ exporter implementation"
Write-Host "  src\adapters\exporters\StlExporter.h    - STL exporter header"
Write-Host "  src\adapters\exporters\StlExporter.cpp  - STL exporter implementation"
Write-Host ""
Write-Host "ADAPTERS - RENDERING:" -ForegroundColor Cyan
Write-Host "  src\adapters\rendering\OcctRenderer.h   - OpenCASCADE renderer header"
Write-Host "  src\adapters\rendering\OcctRenderer.cpp - OpenCASCADE renderer implementation"
Write-Host ""
Write-Host "MAIN:" -ForegroundColor Cyan
Write-Host "  src\main.cpp - Application entry point"
Write-Host ""
Write-Host "============================================" -ForegroundColor Yellow
Write-Host "NEXT STEPS:" -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Yellow
Write-Host "1. Open each file in VS Code or your editor"
Write-Host "2. Replace the placeholder with content from Claude"
Write-Host "3. I'll provide the content for each file in order"
Write-Host ""
Write-Host "Ready to start? Type 'y' when ready for file contents:" -ForegroundColor Green
$ready = Read-Host

if ($ready -eq 'y') {
    Write-Host ""
    Write-Host "Great! Ask Claude for the content of each file, starting with:" -ForegroundColor Cyan
    Write-Host "  'Give me the content for src\domain\Model.h'" -ForegroundColor White
    Write-Host ""
}