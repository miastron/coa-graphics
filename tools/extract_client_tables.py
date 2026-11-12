import argparse
import bz2
import fnmatch
import os
import struct
import sys
import zlib

DESCRIPTION = ("Extract DBFilesClient tables from a 3.3.5 client's MPQ archives into a folder, each from the archive "
               "the client itself would read it from, for tools/convert_client_lights.py and the harness.")
LIGHT_TABLES = ("Light.dbc", "LightIntBand.dbc", "LightFloatBand.dbc", "LightParams.dbc", "LightSkybox.dbc")
TABLE_FOLDER = "DBFilesClient"
DBC_MAGIC = b"WDBC"
DBC_HEADER_FORMAT = "<4s4I"

DATA_FOLDER = "Data"
LOCALE_ARCHIVE_PATTERN = "locale-????.mpq"
NUMBERED_PATCH_PATTERN = "patch-?.mpq"
NUMBERED_LOCALE_PATCH_PATTERN = "patch-{locale}-?.mpq"
FIRST_PATCH = "patch.MPQ"
FIRST_LOCALE_PATCH = "patch-{locale}.MPQ"
BASE_ARCHIVES = (
    "interface.MPQ", "misc.MPQ", "model.MPQ", "texture.MPQ", "terrain.MPQ", "wmo.MPQ", "sound.MPQ", "fonts.MPQ",
    "dbc.MPQ", "speech.MPQ", "expansionloc.MPQ", "lichkingloc.MPQ", "expansionspeech.MPQ", "lichkingspeech.MPQ",
    "expansion.MPQ", "lichking.MPQ", "common.MPQ", "common-2.MPQ", "{locale}\\locale-{locale}.MPQ",
    "{locale}\\speech-{locale}.MPQ", "{locale}\\expansion-locale-{locale}.MPQ",
    "{locale}\\lichking-locale-{locale}.MPQ", "{locale}\\expansion-speech-{locale}.MPQ",
    "{locale}\\lichking-speech-{locale}.MPQ", "development.MPQ")

MPQ_MAGIC = b"MPQ\x1a"
HEADER_ALIGNMENT = 0x200
HEADER_SEARCH_BYTES = 0x100000
HEADER_FORMAT = "<4s2I2H4I"
LARGE_ARCHIVE_FORMAT_VERSION = 1
LARGE_ARCHIVE_HEADER_FORMAT = "<Q2H"
SMALLEST_SECTOR = 0x200
HASH_ENTRY_FORMAT = "<2I2HI"
BLOCK_ENTRY_FORMAT = "<4I"
HASH_TABLE_NAME = "(hash table)"
BLOCK_TABLE_NAME = "(block table)"
HASH_ENTRY_NEVER_USED = 0xFFFFFFFF
HASH_ENTRY_DELETED = 0xFFFFFFFE
NEUTRAL_LOCALE = 0

HASH_TABLE_INDEX = 0
HASH_NAME_A = 1
HASH_NAME_B = 2
HASH_FILE_KEY = 3
HASH_CIPHER = 4
HASH_VALUES_PER_KIND = 0x100
HASH_KINDS = 5
WORD = 0xFFFFFFFF

FILE_IMPLODED = 0x00000100
FILE_COMPRESSED = 0x00000200
FILE_ENCRYPTED = 0x00010000
FILE_KEY_FOLLOWS_POSITION = 0x00020000
FILE_INCREMENTAL_PATCH = 0x00100000
FILE_SINGLE_UNIT = 0x01000000
FILE_DELETION_MARKER = 0x02000000
FILE_SECTOR_CHECKSUMS = 0x04000000
FILE_EXISTS = 0x80000000

PACKED_ZLIB = 0x02
PACKED_BZIP2 = 0x10
UNPACKERS = ((PACKED_BZIP2, bz2.decompress), (PACKED_ZLIB, zlib.decompress))


class ArchiveError(Exception):
    pass


class NotAnArchive(ArchiveError):
    pass


def cipher_table():
    table = [0] * (HASH_VALUES_PER_KIND * HASH_KINDS)
    seed = 0x00100001
    for value in range(HASH_VALUES_PER_KIND):
        for kind in range(HASH_KINDS):
            seed = (seed * 125 + 3) % 0x2AAAAB
            high = (seed & 0xFFFF) << 16
            seed = (seed * 125 + 3) % 0x2AAAAB
            table[kind * HASH_VALUES_PER_KIND + value] = high | (seed & 0xFFFF)
    return table


CIPHER = cipher_table()


def hashed(name, kind):
    seed = 0x7FED7FED
    carry = 0xEEEEEEEE
    for value in name.upper().replace("/", "\\").encode("latin-1"):
        seed = CIPHER[kind * HASH_VALUES_PER_KIND + value] ^ ((seed + carry) & WORD)
        carry = (value + seed + carry + (carry << 5) + 3) & WORD
    return seed


def decrypted(data, key):
    count = len(data) // 4
    carry = 0xEEEEEEEE
    words = []
    for word in struct.unpack_from("<%dI" % count, data):
        carry = (carry + CIPHER[HASH_CIPHER * HASH_VALUES_PER_KIND + (key & 0xFF)]) & WORD
        word ^= (key + carry) & WORD
        key = (((~key << 21) + 0x11111111) & WORD) | (key >> 11)
        carry = (word + carry + (carry << 5) + 3) & WORD
        words.append(word)
    return struct.pack("<%dI" % count, *words) + data[count * 4:]


def unpacked(chunk, size, flags):
    if len(chunk) >= size:
        return chunk[:size]
    if flags & FILE_IMPLODED:
        raise ArchiveError("PKWARE implode is not supported")
    methods = chunk[0]
    data = chunk[1:]
    for method, unpack in UNPACKERS:
        if methods & method:
            data = unpack(data)
            methods &= ~method
    if methods:
        raise ArchiveError("compression method 0x%02X is not supported" % methods)
    if len(data) != size:
        raise ArchiveError("a sector unpacked to %d bytes, not %d" % (len(data), size))
    return data


class Archive:
    def __init__(self, path):
        self.path = path
        self.handle = open(path, "rb")
        self.length = os.path.getsize(path)
        self.start = self.header_position()
        header = self.read_at(0, struct.calcsize(HEADER_FORMAT))
        (_, header_size, _, version, sector_shift, hash_position, block_position, hash_count,
         block_count) = struct.unpack(HEADER_FORMAT, header)
        high_block_position = 0
        if version >= LARGE_ARCHIVE_FORMAT_VERSION:
            extension = self.read_at(len(header), struct.calcsize(LARGE_ARCHIVE_HEADER_FORMAT))
            high_block_position, hash_high, block_high = struct.unpack(LARGE_ARCHIVE_HEADER_FORMAT, extension)
            hash_position |= hash_high << 32
            block_position |= block_high << 32
        self.sector_size = SMALLEST_SECTOR << sector_shift
        self.hash_entries = self.table(hash_position, hash_count, HASH_TABLE_NAME, HASH_ENTRY_FORMAT)
        self.blocks = self.table(block_position, block_count, BLOCK_TABLE_NAME, BLOCK_ENTRY_FORMAT)
        self.high_positions = None
        if high_block_position:
            self.high_positions = struct.unpack("<%dH" % block_count,
                                                self.read_at(high_block_position, block_count * 2))

    def close(self):
        self.handle.close()

    def header_position(self):
        for position in range(0, min(self.length, HEADER_SEARCH_BYTES), HEADER_ALIGNMENT):
            self.handle.seek(position)
            if self.handle.read(len(MPQ_MAGIC)) == MPQ_MAGIC:
                return position
        raise NotAnArchive("no MPQ header")

    def read_at(self, position, size):
        if self.start + position + size > self.length:
            raise ArchiveError("ends at byte %d, before byte %d that it should hold; a partial download?"
                               % (self.length, self.start + position + size))
        self.handle.seek(self.start + position)
        return self.handle.read(size)

    def table(self, position, count, name, entry_format):
        size = struct.calcsize(entry_format)
        data = decrypted(self.read_at(position, count * size), hashed(name, HASH_FILE_KEY))
        return [struct.unpack_from(entry_format, data, index * size) for index in range(count)]

    def block_index(self, name):
        count = len(self.hash_entries)
        if not count:
            return None
        first = hashed(name, HASH_TABLE_INDEX) % count
        name_a = hashed(name, HASH_NAME_A)
        name_b = hashed(name, HASH_NAME_B)
        in_another_locale = None
        for step in range(count):
            entry_a, entry_b, locale, _, block = self.hash_entries[(first + step) % count]
            if block == HASH_ENTRY_NEVER_USED:
                break
            if block == HASH_ENTRY_DELETED or entry_a != name_a or entry_b != name_b:
                continue
            if locale == NEUTRAL_LOCALE:
                return block
            if in_another_locale is None:
                in_another_locale = block
        return in_another_locale

    def block(self, name):
        index = self.block_index(name)
        if index is None:
            return None
        if index >= len(self.blocks):
            raise ArchiveError("%s points at block %d of %d" % (name, index, len(self.blocks)))
        position, packed_size, size, flags = self.blocks[index]
        if not flags & FILE_EXISTS:
            return None
        if self.high_positions:
            position |= self.high_positions[index] << 32
        return position, packed_size, size, flags

    def deletes(self, name):
        block = self.block(name)
        return block is not None and bool(block[3] & FILE_DELETION_MARKER)

    def read(self, name):
        block = self.block(name)
        if block is None:
            return None
        position, packed_size, size, flags = block
        if flags & FILE_INCREMENTAL_PATCH:
            raise ArchiveError("%s is an incremental patch file" % name)
        key = None
        if flags & FILE_ENCRYPTED:
            key = hashed(name.replace("/", "\\").rsplit("\\", 1)[-1], HASH_FILE_KEY)
            if flags & FILE_KEY_FOLLOWS_POSITION:
                key = ((key + position) & WORD) ^ size
        if flags & FILE_SINGLE_UNIT:
            return self.unit(position, packed_size, size, flags, key)
        if flags & (FILE_COMPRESSED | FILE_IMPLODED):
            return self.packed_sectors(position, size, flags, key)
        return self.plain_sectors(position, size, key)

    def unit(self, position, packed_size, size, flags, key):
        data = self.read_at(position, packed_size)
        if key is not None:
            data = decrypted(data, key)
        if flags & (FILE_COMPRESSED | FILE_IMPLODED):
            return unpacked(data, size, flags)
        return data[:size]

    def plain_sectors(self, position, size, key):
        data = self.read_at(position, size)
        if key is None:
            return data
        sectors = [data[start:start + self.sector_size] for start in range(0, size, self.sector_size)]
        return b"".join(decrypted(sector, (key + index) & WORD) for index, sector in enumerate(sectors))

    def packed_sectors(self, position, size, flags, key):
        count = -(-size // self.sector_size)
        entries = count + 1 + (1 if flags & FILE_SECTOR_CHECKSUMS else 0)
        offsets = self.read_at(position, entries * 4)
        if key is not None:
            offsets = decrypted(offsets, (key - 1) & WORD)
        offsets = struct.unpack("<%dI" % entries, offsets)
        sectors = []
        for index in range(count):
            if offsets[index + 1] < offsets[index]:
                raise ArchiveError("sector %d ends before it starts; a wrong key or a damaged file" % index)
            chunk = self.read_at(position + offsets[index], offsets[index + 1] - offsets[index])
            if key is not None:
                chunk = decrypted(chunk, (key + index) & WORD)
            sectors.append(unpacked(chunk, min(self.sector_size, size - index * self.sector_size), flags))
        return b"".join(sectors)


def entries_matching(folder, pattern):
    if not os.path.isdir(folder):
        return []
    return [name for name in os.listdir(folder) if fnmatch.fnmatchcase(name.lower(), pattern.lower())]


def entry_named(folder, name):
    if not os.path.isdir(folder):
        return None
    return next((entry for entry in os.listdir(folder) if entry.lower() == name.lower()), None)


def client_locale(data_folder):
    for name in sorted(os.listdir(data_folder)):
        if entries_matching(os.path.join(data_folder, name), LOCALE_ARCHIVE_PATTERN):
            return name
    raise SystemExit("%s has no locale folder with a %s" % (data_folder, LOCALE_ARCHIVE_PATTERN))


def numbered_patches(data_folder, locale):
    in_data = entries_matching(data_folder, NUMBERED_PATCH_PATTERN)
    locale_pattern = NUMBERED_LOCALE_PATCH_PATTERN.format(locale=locale)
    in_locale = [os.path.join(locale, name)
                 for name in entries_matching(os.path.join(data_folder, locale), locale_pattern)]
    return sorted(in_data + in_locale, key=lambda path: path.lower().replace("/", "\\"), reverse=True)


def existing(data_folder, relative):
    folder, name = os.path.split(relative.replace("\\", os.sep))
    found = entry_named(os.path.join(data_folder, folder), name)
    return os.path.join(folder, found) if found else None


def archives_by_falling_priority(data_folder, locale):
    after_the_numbered = [FIRST_PATCH, os.path.join(locale, FIRST_LOCALE_PATCH)] + list(BASE_ARCHIVES)
    named = [existing(data_folder, name.format(locale=locale)) for name in after_the_numbered]
    return numbered_patches(data_folder, locale) + [name for name in named if name]


class TableCopy:
    def __init__(self, archive, data):
        self.archive = archive
        self.data = data


class ClientArchives:
    def __init__(self, client_folder, locale):
        self.data_folder = os.path.join(client_folder, entry_named(client_folder, DATA_FOLDER) or DATA_FOLDER)
        if not os.path.isdir(self.data_folder):
            raise SystemExit("%s has no %s folder" % (client_folder, DATA_FOLDER))
        self.locale = locale or client_locale(self.data_folder)
        self.names = archives_by_falling_priority(self.data_folder, self.locale)
        self.archives = []
        self.not_archives = []
        self.unreadable = []
        for rank, name in enumerate(self.names):
            try:
                self.archives.append((rank, name, Archive(os.path.join(self.data_folder, name))))
            except NotAnArchive:
                self.not_archives.append(name)
            except (ArchiveError, struct.error, OSError) as error:
                self.unreadable.append((rank, name, str(error)))

    def close(self):
        for _, _, archive in self.archives:
            archive.close()

    def copies(self, table):
        path = TABLE_FOLDER + "\\" + table
        found = []
        for rank, name, archive in self.archives:
            if archive.deletes(path):
                found.append((rank, TableCopy(name, None)))
                continue
            data = archive.read(path)
            if data is not None:
                found.append((rank, TableCopy(name, data)))
        return found


def table_shape(data):
    if len(data) < struct.calcsize(DBC_HEADER_FORMAT):
        return None
    magic, records, _, record_size, strings = struct.unpack_from(DBC_HEADER_FORMAT, data)
    if magic != DBC_MAGIC or len(data) != struct.calcsize(DBC_HEADER_FORMAT) + records * record_size + strings:
        return None
    return records, record_size


def described(copy, chosen):
    if copy.data is None:
        return "%s (deletes it)" % copy.archive
    return "%s (%s)" % (copy.archive, "the same" if copy.data == chosen.data else "differs")


def extract(client, table, output_folder):
    copies = client.copies(table)
    if not copies:
        print("%s: in no archive" % table)
        return False
    rank, chosen = copies[0]
    if chosen.data is None:
        print("%s: deleted by %s" % (table, chosen.archive))
        return False
    shape = table_shape(chosen.data)
    if shape is None:
        print("%s: the copy in %s is not a WDBC table of the size its header states" % (table, chosen.archive))
        return False
    with open(os.path.join(output_folder, table), "wb") as handle:
        handle.write(chosen.data)
    print("%s: %d records of %d bytes from %s" % (table, shape[0], shape[1], chosen.archive))
    if len(copies) > 1:
        print("    also in %s" % ", ".join(described(copy, chosen) for _, copy in copies[1:]))
    outranking = [name for unreadable_rank, name, _ in client.unreadable if unreadable_rank < rank]
    if outranking:
        print("    the client reads %s first, which could not be read here" % ", ".join(outranking))
    return not outranking


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("client", help="the client folder, the one that holds %s" % DATA_FOLDER)
    parser.add_argument("output", help="folder to write the tables into")
    parser.add_argument("--tables", nargs="+", default=list(LIGHT_TABLES), metavar="TABLE",
                        help="table names in %s (default: %s)" % (TABLE_FOLDER, " ".join(LIGHT_TABLES)))
    parser.add_argument("--locale", help="the locale folder in %s (default: the one with a locale archive)"
                        % DATA_FOLDER)
    arguments = parser.parse_args()
    client = ClientArchives(arguments.client, arguments.locale)
    os.makedirs(arguments.output, exist_ok=True)
    print("%d archives of locale %s, by falling priority: %s"
          % (len(client.names), client.locale, ", ".join(client.names)))
    for name in client.not_archives:
        print("not an MPQ archive, so the client skips it too: %s" % name)
    for _, name, reason in client.unreadable:
        print("unreadable: %s: %s" % (name, reason))
    complete = [extract(client, table, arguments.output) for table in arguments.tables]
    client.close()
    return 0 if all(complete) else 1


if __name__ == "__main__":
    sys.exit(main())
