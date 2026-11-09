from bisect import bisect_left
from pathlib import Path
import re
import sys

FACTORY = re.compile(r"auto connector\s*=\s*qwertycoin::epose::service_probe_connector_v2\(")
BINDING = re.compile(r"(?<![\w.>:])client\.set_connector\s*\(\s*std::move\(connector\)\s*\)")
QUALIFIED_RECEIVER = re.compile(r"(?:\.|->|::)\s*$")
LINE_SPLICE = re.compile(r"\\[ \t\v\f]*\r?\n")
RAW_OPENER = re.compile(r'R"(?P<delimiter>[^ ()\\\t\r\n"]{0,16})\(')


def check_probe(probe):
    non_code = re.compile(
        RAW_OPENER.pattern + r'[\s\S]*?\)(?P=delimiter)"'
        r'|"(?:\\[\s\S]|[^"\\])*"'
        r"|(?<!\w)(?:u8|[uUL])?'(?:\\[\s\S]|[^'\\])*'"
        r"|/\*[\s\S]*?\*/|//(?:\\[ \t\v\f]*\r?\n|[^\n])*"
    )
    probe = normalize_line_splices(probe, non_code)
    active_probe = non_code.sub(lambda match: mask_non_code(match.group()), probe)
    if any(line.lstrip().startswith("#") for line in active_probe.split("\n")):
        raise ValueError("preprocessor directive in service probe region")
    factories = FACTORY.findall(active_probe)
    if len(factories) != 1:
        raise ValueError("service probe connector factory is missing or duplicated")
    if not has_probe_client_binding(active_probe):
        raise ValueError("service probe connector is not installed on the HTTP client")


def has_probe_client_binding(probe):
    return any(not QUALIFIED_RECEIVER.search(probe[:match.start()])
               for match in BINDING.finditer(probe))


def normalize_line_splices(probe, non_code):
    pieces = []
    positions = []
    start = 0
    for splice in LINE_SPLICE.finditer(probe):
        pieces.append(probe[start:splice.start()])
        positions.extend(range(start, splice.start()))
        start = splice.end()
    if start == 0:
        return probe
    pieces.append(probe[start:])
    positions.extend(range(start, len(probe)))
    normalized = "".join(pieces)
    protected = []
    start = 0
    for match in non_code.finditer(normalized):
        if match.start() < start or not match.group().startswith('R"'):
            continue
        opener = RAW_OPENER.match(normalized, match.start())
        if opener is None:
            continue
        opener_end = opener.end()
        body_start = positions[opener_end - 1] + 1
        closer = ')' + opener.group('delimiter') + '"'
        close_start = probe.find(closer, body_start)
        if close_start < 0:
            continue
        close_end = close_start + len(closer)
        protected.extend((normalized[start:match.start()], normalized[match.start():opener_end],
                          probe[body_start:close_end]))
        start = bisect_left(positions, close_end)
    protected.append(normalized[start:])
    return "".join(protected)


def mask_non_code(text):
    return re.sub(r"[^\n]", " ", text)


if __name__ == "__main__":
    root = Path(sys.argv[1])
    source = (root / "src/cryptonote_core/epose_http_client.cpp").read_text()
    start = source.index("receipt_step_result_v2 request_epose_service_response(")
    end = source.index("COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::request request{}", start)
    try:
        check_probe(source[start:end])
    except ValueError as error:
        raise SystemExit(str(error))
