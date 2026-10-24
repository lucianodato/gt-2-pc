#include "gt2formats/save_data.h"

#include <cstring>
#include <stdexcept>

#include "gt2formats/overlay_data.h"

namespace gt2 {

namespace {

SaveNames g_saveNames;

std::string ExeString(const GuestImage& exe, uint32_t address) {
    uint32_t at = address;
    try {
        if (!exe.unknownBuild && exe.profile) at = exe.Sim(address);
        std::string s;
        for (size_t i = 0; i < 24; i++) {
            const char c = char(exe.Get<uint8_t>(at + uint32_t(i)));
            if (c == 0) break;
            s.push_back(c);
        }
        return s;
    } catch (const std::exception&) {
        return {};
    }
}

uint32_t U32(std::span<const uint8_t> b, size_t o) {
    if (o + 4 > b.size()) throw std::out_of_range("save data: read past the end");
    return uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24;
}
uint16_t U16(std::span<const uint8_t> b, size_t o) {
    if (o + 2 > b.size()) throw std::out_of_range("save data: read past the end");
    return uint16_t(b[o] | b[o + 1] << 8);
}

} // namespace

SaveNames LoadSaveNames(const GuestImage& exe) {
    SaveNames n;
    const std::string game = ExeString(exe, 0x80091C94u);
    const std::string replay = ExeString(exe, 0x80091AB8u);
    const std::string arcade = ExeString(exe, 0x8009198Cu);
    if (!game.empty()) n.game = game;
    if (!replay.empty()) n.replay = replay;
    if (!arcade.empty()) n.arcade = arcade;
    return n;
}

const SaveNames& CurrentSaveNames() { return g_saveNames; }
void SetSaveNames(SaveNames names) {
    if (names.game.empty()) names.game = "BASCUS-94455GAME";
    if (names.replay.empty()) names.replay = "BASCUS-94455REPLAY";
    if (names.arcade.empty()) names.arcade = names.game;
    g_saveNames = std::move(names);
}

bool SaveFileNameMatches(const std::string& name, const std::string& wanted) {
    if (name == wanted) return true;
    return (wanted == "BASCUS-94455GAME" && name.rfind(wanted, 0) == 0) ||
           (wanted == "BASCUS-94455REPLAY" && name.rfind(wanted, 0) == 0);
}

std::vector<MemoryCardFile> ReadMemoryCardFiles(std::span<const uint8_t> image) {
    constexpr size_t kFrame = 128, kBlock = 8192, kBlocks = 15;
    if (image.size() < kBlock * 16 || image[0] != 'M' || image[1] != 'C') throw std::runtime_error("not a PS1 memory card image");
    std::vector<MemoryCardFile> files;
    for (size_t i = 1; i <= kBlocks; i++) {
        const std::span<const uint8_t> dir = image.subspan(i * kFrame, kFrame);
        if (U32(dir, 0) != 0x51) continue; // first block of a file in use
        MemoryCardFile f;
        f.size = U32(dir, 4);
        f.firstBlock = int(i);
        const char* name = reinterpret_cast<const char*>(dir.data() + 10);
        f.name.assign(name, strnlen(name, 20));
        size_t block = i;
        for (size_t guard = 0; guard < kBlocks && f.bytes.size() < f.size; guard++) {
            const std::span<const uint8_t> data = image.subspan(block * kBlock, kBlock);
            f.bytes.insert(f.bytes.end(), data.begin(), data.end());
            const uint16_t next = U16(image.subspan(block * kFrame, kFrame), 8);
            if (next == 0xFFFF) break;
            block = size_t(next) + 1;
            if (block < 1 || block > kBlocks) break;
        }
        if (f.bytes.size() > f.size) f.bytes.resize(f.size);
        files.push_back(std::move(f));
    }
    return files;
}

uint32_t Crc32(std::span<const uint8_t> bytes) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t x = i;
            for (int k = 0; k < 8; k++) x = (x & 1) ? (x >> 1) ^ 0xEDB88320u : x >> 1;
            t[i] = x;
        }
        return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (uint8_t b : bytes) crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

GarageCar ParseGarageCar(std::span<const uint8_t> slot) {
    if (slot.size() < kGarageSlotSize) throw std::out_of_range("save data: short garage slot");
    GarageCar c;
    c.carId = U32(slot, 0x00);
    c.colour = U32(slot, 0x04);
    std::memcpy(&c.config, slot.data() + 0x08, sizeof(CarConfig));
    c.modelId = U32(slot, 0x8C);
    c.price = U32(slot, 0x90);
    const uint16_t w94 = U16(slot, 0x94), w98 = U16(slot, 0x98);
    c.weightKg = w94 & 0x1FFF;
    c.driveType = uint8_t(w94 >> 13);
    c.word96 = U16(slot, 0x96);
    c.power = w98 & 0x3FFF;
    c.flag14 = (w98 >> 14) & 1;
    c.flag15 = (w98 >> 15) & 1;
    std::memcpy(c.partsOwned.data(), slot.data() + 0x9A, c.partsOwned.size());
    c.wordA2 = U16(slot, 0xA2);
    return c;
}

CareerRecord GameSave::Career() const {
    const std::span<const uint8_t> r = std::span<const uint8_t>(state).subspan(kStateCareerRecord, 0x160);
    CareerRecord c;
    c.days = U32(r, 0x40);
    c.word48 = U32(r, 0x48);
    c.wins = U32(r, 0x4C);
    c.positionSum = U32(r, 0x50);
    c.races = U32(r, 0x54);
    c.prizeHundredMillions = U32(r, 0x58);
    c.prize = U32(r, 0x5C);
    c.resultNibbles.assign(r.begin() + 0x60, r.end());
    return c;
}

Garage GameSave::GarageBlock() const {
    const std::span<const uint8_t> g = std::span<const uint8_t>(state).subspan(kStateGarage, kGarageBlockSize);
    Garage out;
    out.count = U16(g, 0);
    if (out.count > kGarageSlots) throw std::runtime_error("save data: garage count > 100");
    for (size_t i = 0; i < out.count; i++) out.cars.push_back(ParseGarageCar(g.subspan(4 + i * kGarageSlotSize, kGarageSlotSize)));
    out.money = U32(g, 0x4014);
    out.currentCar = int16_t(U16(g, 0x4018));
    out.byte401B = g[0x401B];
    return out;
}

GameSave ParseGameSave(std::span<const uint8_t> file) {
    if (file.size() < kSaveFileSize) throw std::runtime_error("save data: file shorter than 0x7EA0 bytes");
    if (file[0] != 'S' || file[1] != 'C') throw std::runtime_error("save data: no 'SC' header");
    GameSave s;
    s.header.assign(file.begin(), file.begin() + kSaveHeaderSize);
    s.state.assign(file.begin() + kSaveHeaderSize, file.begin() + kSaveHeaderSize + kSaveStateSize);
    s.storedCrc = U32(file, kSaveCrcOffset);
    s.computedCrc = Crc32(file.first(kSaveCrcOffset));
    return s;
}

GameSave GameSaveFromState(std::span<const uint8_t> state) {
    if (state.size() < kSaveStateSize) throw std::runtime_error("save data: state shorter than 0x7C9C bytes");
    GameSave s;
    s.header.assign(kSaveHeaderSize, 0);
    s.state.assign(state.begin(), state.begin() + kSaveStateSize);
    return s;
}

std::vector<uint8_t> BuildGameSaveFile(std::span<const uint8_t> header, std::span<const uint8_t> state) {
    if (header.size() != kSaveHeaderSize || state.size() != kSaveStateSize) throw std::runtime_error("save data: bad section sizes");
    std::vector<uint8_t> file(kSaveFileSize, 0);
    std::memcpy(file.data(), header.data(), kSaveHeaderSize);
    std::memcpy(file.data() + kSaveHeaderSize, state.data(), kSaveStateSize);
    const uint32_t crc = Crc32(std::span<const uint8_t>(file).first(kSaveCrcOffset));
    for (int i = 0; i < 4; i++) file[kSaveCrcOffset + i] = uint8_t(crc >> (8 * i));
    return file;
}

} // namespace gt2
