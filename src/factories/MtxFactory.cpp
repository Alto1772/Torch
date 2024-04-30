#include "MtxFactory.h"
#include "spdlog/spdlog.h"

#include "Companion.h"
#include "utils/Decompressor.h"

#define NUM(x) std::dec << std::setfill(' ') << std::setw(6) << x
#define COL(c) std::dec << std::setfill(' ') << std::setw(3) << c

ExportResult MtxHeaderExporter::Export(std::ostream &write, std::shared_ptr<IParsedData> raw, std::string& entryName, YAML::Node &node, std::string* replacement) {
    const auto symbol = GetSafeNode(node, "symbol", entryName);

    if(Companion::Instance->IsOTRMode()){
        write << "static const ALIGN_ASSET(2) char " << symbol << "[] = \"__OTR__" << (*replacement) << "\";\n\n";
        return std::nullopt;
    }

    const auto offset = GetSafeNode<uint32_t>(node, "offset");
    const auto searchTable = Companion::Instance->SearchTable(offset);

    if(searchTable.has_value()){
        const auto [name, start, end, mode, index_size] = searchTable.value();

        if(start != offset){
            return std::nullopt;
        }

        write << "extern Mtx " << name << "[];\n";
    } else {
        write << "extern Mtx " << symbol << ";\n";
    }
    return std::nullopt;
}

ExportResult MtxCodeExporter::Export(std::ostream &write, std::shared_ptr<IParsedData> raw, std::string& entryName, YAML::Node &node, std::string* replacement ) {
    auto& m = std::static_pointer_cast<MtxData>(raw)->mMtx;
    const auto symbol = GetSafeNode(node, "symbol", entryName);
    const auto offset = GetSafeNode<uint32_t>(node, "offset");
    const auto searchTable = Companion::Instance->SearchTable(offset);
    const bool isInTable = searchTable.has_value();

    /**
     * toFixedPointMatrix(
     *     1.0, 0.0, 0.0, 0.0,
     *     0.0, 1.0, 0.0, 0.0,
     *     0.0, 0.0, 1.0, 0.0,
     *     0.0, 0.0, 0.0, 1.0
     * );
     */

    if (isInTable) {
        if (searchTable.value().start == offset) {
            write << "Mtx " << searchTable.value().name << "[] = {\n";
        }
        write << fourSpaceTab << "toFixedPointMatrix(\n";
    } else {
        write << "Mtx " << symbol << " = toFixedPointMatrix(\n";
    }

    for (int j = 0; j < 16; ++j) {

        // Add spaces at the start of line
        if (j % 4 == 0) {
            if (isInTable) {
                write << fourSpaceTab << fourSpaceTab;
            } else {
                write << fourSpaceTab;
            }
        }

        // Turn 1, 3, and 6 into 1.0, 3.0, and 6.0. Unless it has a decimal number then leave it alone.
        SPDLOG_INFO(m.mtx[j]);
        if (std::abs(m.mtx[j] - static_cast<int>(m.mtx[j])) < 1e-6) {
            write << std::fixed << std::setprecision(1) << m.mtx[j];
        } else {
            // Stupid hack to get matching precision so this value outputs 0.0000153 instead.
            if (std::fabs(m.mtx[j] - 0.000015) < 0.000001) {
                write << std::fixed << std::setprecision(7) << m.mtx[j];
            } else {
                write << std::fixed << std::setprecision(6) << m.mtx[j];
            }
        }

        // Add comma for all but the last arg
        if (j < 15) {
            write << ", ";
        }

        // Add newline after every four args
        if ((j + 1) % 4 == 0) {
            write << "\n";
        }
    }

    if (isInTable) {
        write << fourSpaceTab << "),\n";
    } else {
        write << ");\n";
    }

    if (isInTable && searchTable.value().end == offset) {
        write << "};\n";
    }

    return offset + sizeof(MtxRaw);
}

ExportResult MtxBinaryExporter::Export(std::ostream &write, std::shared_ptr<IParsedData> raw, std::string& entryName, YAML::Node &node, std::string* replacement ) {
    auto& m = std::static_pointer_cast<MtxData>(raw)->mMtx;
    auto writer = LUS::BinaryWriter();

    WriteHeader(writer, LUS::ResourceType::Matrix, 0);

    writer.Write(m.mtx[0]);
    writer.Write(m.mtx[1]);
    writer.Write(m.mtx[2]);
    writer.Write(m.mtx[3]);
    writer.Write(m.mtx[4]);
    writer.Write(m.mtx[5]);
    writer.Write(m.mtx[6]);
    writer.Write(m.mtx[7]);
    writer.Write(m.mtx[8]);
    writer.Write(m.mtx[9]);
    writer.Write(m.mtx[10]);
    writer.Write(m.mtx[11]);
    writer.Write(m.mtx[12]);
    writer.Write(m.mtx[13]);
    writer.Write(m.mtx[14]);
    writer.Write(m.mtx[15]);

    //throw std::runtime_error("Mtx not tested for otr/o2r.");
    writer.Finish(write);
    return std::nullopt;
}

std::optional<std::shared_ptr<IParsedData>> MtxFactory::parse(std::vector<uint8_t>& buffer, YAML::Node& node) {
    auto [_, segment] = Decompressor::AutoDecode(node, buffer);
    LUS::BinaryReader reader(segment.data, sizeof(MtxRaw));

    reader.SetEndianness(LUS::Endianness::Big);

    #define FIXTOF(x)      ((float)((x) / 65536.0f))

    // Reads the integer portion, the fractional portion, puts each together into a fixed-point value, and finally converts to float.

    // Read the integer portion of the fixed-point value (ex. 4)
    auto i1  = reader.ReadInt16();
    auto i2  = reader.ReadInt16();
    auto i3  = reader.ReadInt16();
    auto i4  = reader.ReadInt16();
    auto i5  = reader.ReadInt16();
    auto i6  = reader.ReadInt16();
    auto i7  = reader.ReadInt16();
    auto i8  = reader.ReadInt16();
    auto i9  = reader.ReadInt16();
    auto i10 = reader.ReadInt16();
    auto i11 = reader.ReadInt16();
    auto i12 = reader.ReadInt16();
    auto i13 = reader.ReadInt16();
    auto i14 = reader.ReadInt16();
    auto i15 = reader.ReadInt16();
    auto i16 = reader.ReadInt16();

    // Read the fractional portion of the fixed-point value (ex. 0.45)
    auto f1  = reader.ReadUInt16();
    auto f2  = reader.ReadUInt16();
    auto f3  = reader.ReadUInt16();
    auto f4  = reader.ReadUInt16();
    auto f5  = reader.ReadUInt16();
    auto f6  = reader.ReadUInt16();
    auto f7  = reader.ReadUInt16();
    auto f8  = reader.ReadUInt16();
    auto f9  = reader.ReadUInt16();
    auto f10 = reader.ReadUInt16();
    auto f11 = reader.ReadUInt16();
    auto f12 = reader.ReadUInt16();
    auto f13 = reader.ReadUInt16();
    auto f14 = reader.ReadUInt16();
    auto f15 = reader.ReadUInt16();
    auto f16 = reader.ReadUInt16();

    // Place the integer and fractional portions together (ex 4.45) and convert to floating-point
    auto m1  = FIXTOF( (int32_t) ( (i1 << 16) | f1 ) );
    auto m2  = FIXTOF( (int32_t) ( (i2 << 16) | f2 ) );
    auto m3  = FIXTOF( (int32_t) ( (i3 << 16) | f3 ) );
    auto m4  = FIXTOF( (int32_t) ( (i4 << 16) | f4 ) );
    auto m5  = FIXTOF( (int32_t) ( (i5 << 16) | f5 ) );
    auto m6  = FIXTOF( (int32_t) ( (i6 << 16) | f6 ) );
    auto m7  = FIXTOF( (int32_t) ( (i7 << 16) | f7 ) );
    auto m8  = FIXTOF( (int32_t) ( (i8 << 16) | f8 ) );
    auto m9  = FIXTOF( (int32_t) ( (i9 << 16) | f9 ) );
    auto m10 = FIXTOF( (int32_t) ( (i10 << 16) | f10 ) );
    auto m11 = FIXTOF( (int32_t) ( (i11 << 16) | f11 ) );
    auto m12 = FIXTOF( (int32_t) ( (i12 << 16) | f12 ) );
    auto m13 = FIXTOF( (int32_t) ( (i13 << 16) | f13 ) );
    auto m14 = FIXTOF( (int32_t) ( (i14 << 16) | f14 ) );
    auto m15 = FIXTOF( (int32_t) ( (i15 << 16) | f15 ) );
    auto m16 = FIXTOF( (int32_t) ( (i16 << 16) | f16 ) );


    auto matrix = MtxRaw({
        m1, m2, m3, m4,
        m5, m6, m7, m8,
        m9, m10, m11, m12,
        m13, m14, m15, m16,
    });

    #undef FIXTOF

    return std::make_shared<MtxData>(matrix);
}
