"""Shared dataclasses for firmware size reports.

Both `parse_size.py` (producer, writes JSON) and `render_size_comment.py`
(consumer, reads JSON) speak this schema. Keeping it in one file means the
consumer doesn't have to re-derive the shape from raw dicts.

The on-disk format is `dataclasses.asdict(board)` — each `*.size.json`
artifact deserialises back into a `BoardSize` via `BoardSize.from_dict`.
"""
from dataclasses import dataclass


@dataclass
class Section:
    used_bytes: int
    total_bytes: int

    @classmethod
    def from_dict(cls, d: dict | None) -> "Section | None":
        if not d:
            return None
        return cls(used_bytes=d["used_bytes"], total_bytes=d["total_bytes"])


@dataclass
class BoardSize:
    schema_version: int
    pio_env: str
    board_name: str
    sha: str
    flash: Section | None
    ram: Section | None

    @classmethod
    def from_dict(cls, d: dict) -> "BoardSize":
        return cls(
            schema_version=d["schema_version"],
            pio_env=d["pio_env"],
            board_name=d["board_name"],
            sha=d["sha"],
            flash=Section.from_dict(d.get("flash")),
            ram=Section.from_dict(d.get("ram")),
        )
