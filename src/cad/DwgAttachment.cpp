#include "workstation/cad/DwgAttachment.h"
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/AciColorTable.h"

#include <fstream>
#include <cmath>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif

namespace workstation {
namespace cad {

static constexpr double kPi = 3.14159265358979323846;

DwgAttachment::DwgAttachment(const std::string& filepath)
    : m_filePath(filepath), m_reader(std::make_unique<DxfFileReader>()) {}

std::string DwgAttachment::filename() const {
    auto pos = m_filePath.find_last_of("/\\");
    return (pos != std::string::npos) ? m_filePath.substr(pos + 1) : m_filePath;
}

bool DwgAttachment::load(std::string* errorMessage) {
    std::string prjPath = findAdjacentPrj(m_filePath);
    if (!prjPath.empty()) {
        m_prjBlocks = parsePrjBlocks(prjPath);
    }

    if (loadViaDirectRead(errorMessage)) return true;
    if (loadViaOdaConverter(errorMessage)) return true;

    if (errorMessage) *errorMessage = "Cannot load DWG file: " + m_filePath;
    return false;
}

bool DwgAttachment::loadViaDirectRead(std::string* errorMessage) {
    std::ifstream file(m_filePath, std::ios::binary);
    if (!file) {
        if (errorMessage) *errorMessage = "Cannot open DWG file";
        return false;
    }

    char header[32] = {};
    file.read(header, 32);
    file.close();

    bool isDxfText = false;
    if (header[0] >= '0' && header[0] <= '9') isDxfText = true;

    if (isDxfText) {
        bool ok = m_reader->readFile(m_filePath, errorMessage);
        if (ok) {
            const auto& doc = m_reader->document();
            m_layerManager.clear();
            const auto& aci = AciColorTable::Instance();
            for (const auto& ent : doc.entities) {
                std::string layer = ent.layer.empty() ? "0" : ent.layer;
                uint8_t r, g, b;
                if (ent.hasTrueColor) { auto rgb = aci.TrueColorToRgb(ent.trueColor); r = rgb.r; g = rgb.g; b = rgb.b; }
                else { auto [rr, gg, bb] = aci.LookupNormalized(ent.color); r = rr * 255; g = gg * 255; b = bb * 255; }
                m_layerManager.addLayer(layer, r, g, b, 1);
            }
            m_loaded = true;
        }
        return ok;
    }

    if (errorMessage) *errorMessage = "DWG binary format requires ODA File Converter";
    return false;
}

bool DwgAttachment::loadViaOdaConverter(std::string* errorMessage) {
#ifdef _WIN32
    const char* odaPaths[] = {
        "C:\\Program Files\\ODA\\ODAFileConverter\\ODAFileConverter.exe",
        "C:\\Program Files (x86)\\ODA\\ODAFileConverter\\ODAFileConverter.exe",
    };

    std::string odaExe;
    for (const auto& p : odaPaths) {
        std::ifstream test(p);
        if (test.good()) { odaExe = p; break; }
    }

    if (odaExe.empty()) {
        if (errorMessage) *errorMessage = "ODA File Converter not found";
        return false;
    }

    char tempDir[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDir);
    std::string inputDir = std::string(tempDir) + "\\wk_dwg_input";
    std::string outputDir = std::string(tempDir) + "\\wk_dwg_output";

    CreateDirectoryA(inputDir.c_str(), NULL);
    CreateDirectoryA(outputDir.c_str(), NULL);

    std::string destFile = inputDir + "\\" + filename();
    CopyFileA(m_filePath.c_str(), destFile.c_str(), FALSE);

    std::string cmd = "\"" + odaExe + "\" \"" + inputDir + "\" \"" + outputDir + "\" ACAD2018 DXF 0 1 1";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION procInfo = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &procInfo)) {
        if (errorMessage) *errorMessage = "Failed to run ODA File Converter";
        return false;
    }

    WaitForSingleObject(procInfo.hProcess, 60000);
    DWORD exitCode = 0;
    GetExitCodeProcess(procInfo.hProcess, &exitCode);
    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);

    if (exitCode != 0) {
        if (errorMessage) *errorMessage = "ODA conversion failed with exit code " + std::to_string(exitCode);
        return false;
    }

    std::string dxfPath;
    WIN32_FIND_DATAA findData;
    std::string pattern = outputDir + "\\*.dxf";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        dxfPath = outputDir + "\\" + findData.cFileName;
        FindClose(hFind);
    }

    if (dxfPath.empty()) {
        if (errorMessage) *errorMessage = "ODA produced no DXF output";
        return false;
    }

    bool ok = m_reader->readFile(dxfPath, errorMessage);
    if (ok) {
        const auto& doc = m_reader->document();
        m_layerManager.clear();
        const auto& aci = AciColorTable::Instance();
        for (const auto& ent : doc.entities) {
            std::string layer = ent.layer.empty() ? "0" : ent.layer;
            uint8_t r, g, b;
            if (ent.hasTrueColor) { auto rgb = aci.TrueColorToRgb(ent.trueColor); r = rgb.r; g = rgb.g; b = rgb.b; }
            else { auto [rr, gg, bb] = aci.LookupNormalized(ent.color); r = rr * 255; g = gg * 255; b = bb * 255; }
            m_layerManager.addLayer(layer, r, g, b, 1);
        }
        m_loaded = true;
    }

    RemoveDirectoryA(inputDir.c_str());
    RemoveDirectoryA(outputDir.c_str());

    return ok;
#else
    if (errorMessage) *errorMessage = "DWG loading via ODA is only supported on Windows";
    return false;
#endif
}

std::vector<PrjBlock> DwgAttachment::parsePrjBlocks(const std::string& prjPath) {
    std::vector<PrjBlock> blocks;
    std::ifstream file(prjPath);
    if (!file) return blocks;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    PrjBlock currentBlock;
    bool inBlock = false;
    std::vector<std::pair<double, double>> currentPoly;
    double currentX = 0, currentY = 0;

    size_t pos = 0;
    while (pos < content.size()) {
        size_t lineEnd = content.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = content.size();
        std::string line = content.substr(pos, lineEnd - pos);
        pos = lineEnd + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();

        if (line.find("INSERT") != std::string::npos) {
            if (inBlock && !currentPoly.empty()) {
                currentBlock.polygon = currentPoly;
                blocks.push_back(currentBlock);
            }
            currentBlock = PrjBlock{};
            currentPoly.clear();
            inBlock = true;
        }
        else if (!line.empty() && line[0] == 'X' && line.size() > 1) {
            currentX = std::stod(line.substr(1), nullptr);
        }
        else if (!line.empty() && line[0] == 'Y' && line.size() > 1) {
            currentY = std::stod(line.substr(1), nullptr);
            currentPoly.push_back({currentX, currentY});
        }
    }

    if (inBlock && !currentPoly.empty()) {
        currentBlock.polygon = currentPoly;
        blocks.push_back(currentBlock);
    }

    for (auto& block : blocks) {
        if (!block.polygon.empty()) {
            double cx = 0, cy = 0;
            for (const auto& [x, y] : block.polygon) { cx += x; cy += y; }
            block.centroidX = cx / block.polygon.size();
            block.centroidY = cy / block.polygon.size();
        }
    }

    return blocks;
}

std::string DwgAttachment::findAdjacentPrj(const std::string& dwgPath) {
    auto lastSlash = dwgPath.find_last_of("/\\");
    std::string dir = (lastSlash != std::string::npos) ? dwgPath.substr(0, lastSlash) : ".";
    auto dotPos = dwgPath.find_last_of('.');
    std::string stem = (dotPos != std::string::npos) ? dwgPath.substr(lastSlash + 1, dotPos - lastSlash - 1) : dwgPath.substr(lastSlash + 1);

    std::string sameStem = dir + "\\" + stem + ".prj";
    std::ifstream test(sameStem);
    if (test.good()) return sameStem;

    std::string bakPath = sameStem + ".bak";
    std::ifstream testBak(bakPath);
    if (testBak.good()) return bakPath;

    return {};
}

DxfAttachmentGeometry DwgAttachment::buildGeometry() const {
    if (!m_loaded || !m_reader) return {};
    DxfAttachmentGeometry geom;
    const auto& doc = m_reader->document();
    const auto& aci = AciColorTable::Instance();

    auto addSegment = [&](float x0, float y0, float z0, float x1, float y1, float z1, uint8_t r, uint8_t g, uint8_t b) {
        uint32_t base = static_cast<uint32_t>(geom.lineVertices.size() / 3);
        geom.lineVertices.insert(geom.lineVertices.end(), {x0, y0, z0 + static_cast<float>(m_zOffset)});
        geom.lineVertices.insert(geom.lineVertices.end(), {x1, y1, z1 + static_cast<float>(m_zOffset)});
        geom.lineIndices.insert(geom.lineIndices.end(), {base, base + 1});
        float fr = r / 255.0f, fg = g / 255.0f, fb = b / 255.0f;
        geom.lineColors.insert(geom.lineColors.end(), {fr, fg, fb});
        geom.lineColors.insert(geom.lineColors.end(), {fr, fg, fb});
    };

    for (const auto& ent : doc.entities) {
        if (!m_layerManager.isLayerVisible(ent.layer)) continue;
        uint8_t r, g, b;
        if (ent.hasTrueColor) { auto rgb = aci.TrueColorToRgb(ent.trueColor); r = rgb.r; g = rgb.g; b = rgb.b; }
        else { auto [rr, gg, bb] = aci.LookupNormalized(ent.color); r = rr * 255; g = gg * 255; b = bb * 255; }

        if (ent.type == "LINE" && ent.points.size() >= 2) {
            addSegment(ent.points[0].x, ent.points[0].y, ent.points[0].z,
                       ent.points[1].x, ent.points[1].y, ent.points[1].z, r, g, b);
        }
        else if (ent.type == "LWPOLYLINE" || ent.type == "POLYLINE") {
            int n = static_cast<int>(ent.points.size());
            for (int i = 0; i < n - 1; ++i)
                addSegment(ent.points[i].x, ent.points[i].y, ent.points[i].z,
                           ent.points[i+1].x, ent.points[i+1].y, ent.points[i+1].z, r, g, b);
            if (ent.closed && n > 2)
                addSegment(ent.points[n-1].x, ent.points[n-1].y, ent.points[n-1].z,
                           ent.points[0].x, ent.points[0].y, ent.points[0].z, r, g, b);
        }
        else if (ent.type == "CIRCLE") {
            auto pts = DxfFileReader::generateCirclePoints(ent.center, ent.radius, 64);
            for (size_t i = 0; i < pts.size(); ++i) {
                size_t next = (i + 1) % pts.size();
                addSegment(pts[i].x, pts[i].y, pts[i].z, pts[next].x, pts[next].y, pts[next].z, r, g, b);
            }
        }
        else if (ent.type == "ARC") {
            auto pts = DxfFileReader::generateArcPoints(ent.center, ent.radius, ent.startAngle, ent.endAngle);
            for (size_t i = 0; i + 1 < pts.size(); ++i)
                addSegment(pts[i].x, pts[i].y, pts[i].z, pts[i+1].x, pts[i+1].y, pts[i+1].z, r, g, b);
        }
    }
    return geom;
}

} // namespace cad
} // namespace workstation
