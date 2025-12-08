#!/usr/bin/env python3
import argparse
import csv
import os
import twlang

from typing import Dict, Any


class DDNetEntry:
    def __init__(self, source, context, target, **_kwargs):
        self.source = source
        self.context = context
        self.target = target

    def __str__(self):
        if self.context != "":
            return f"[{self.context}]\n{self.source}\n== {self.target}"
        else:
            return f"{self.source}\n== {self.target}"


def handle_translation_file(entry: os.DirEntry[str], mapping: Dict[str, Any], prefix: str):
    with open(entry.path, encoding="utf-8") as f:
        entry_list = [DDNetEntry(**entry) for entry in csv.DictReader(f)]

    # tuple sort by context -> source -> target
    entry_list_sorted = sorted(entry_list, key=lambda dd_entry: (
        dd_entry.context, dd_entry.source, dd_entry.target))
    
    out = "\n".join([str(dd_entry) for dd_entry in entry_list_sorted])
    key = mapping[entry.name]
    with open(f"{prefix}/data/languages/{key}.txt", "w", encoding="utf-8") as f:
        f.write(out)


def main():
    parser = argparse.ArgumentParser(
        description="Convert Weblate CSV files in the specified directory into DDNet translation files.")
    parser.add_argument("in_dir", metavar="IN_DIR", help="Directory with the Weblate CSV files")
    args = parser.parse_args()

    prefix = os.path.dirname(__file__) + "/../.."

    mapping = {f"{rfc3066}.csv": key for key, (_, _, _, rfc3066, _) in twlang.language_index().items() for rfc3066 in
               rfc3066.split(";")}

    for entry in sorted(os.scandir(args.in_dir), key=lambda e: e.name):
        handle_translation_file(entry, mapping, prefix)


if __name__ == "__main__":
    main()
