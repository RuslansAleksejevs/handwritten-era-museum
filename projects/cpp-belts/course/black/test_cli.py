"""Independent JSON/XML/protobuf boundary checks; no datasets or network."""
import copy
import json
import subprocess
import xml.etree.ElementTree as ET


def run(binary, mode, payload, *args, raw=False, valid=True):
    data = payload if isinstance(payload, bytes) else json.dumps(payload).encode()
    result = subprocess.run([str(binary), mode, *args], input=data, capture_output=True, timeout=15)
    if not valid:
        assert result.returncode != 0, result.stdout
        return
    assert result.returncode == 0, result.stderr.decode()
    return result.stdout if raw else json.loads(result.stdout)


def transport_fixture():
    return {
        "base_requests": [
            {"type": "Stop", "name": "A & <", "latitude": 1, "longitude": 1, "road_distances": {"B": 1000}},
            {"type": "Stop", "name": "B", "latitude": 2, "longitude": 3, "road_distances": {"A & <": 1200, "C": 2000}},
            {"type": "Stop", "name": "C", "latitude": 3, "longitude": 2, "road_distances": {"B": 2500}},
            {"type": "Stop", "name": "Unused", "latitude": 1.5, "longitude": 2.5, "road_distances": {}},
            {"type": "Bus", "name": "X", "stops": ["A & <", "B", "C"], "is_roundtrip": False},
        ],
        "routing_settings": {"bus_wait_time": 2, "bus_velocity": 60},
        "render_settings": {
            "width": 400, "height": 300, "padding": 20, "stop_radius": 5, "line_width": 3,
            "stop_label_font_size": 14, "stop_label_offset": [7, -3],
            "bus_label_font_size": 16, "bus_label_offset": [7, 14],
            "underlayer_color": [255, 255, 255, .85], "underlayer_width": 3,
            "color_palette": ["teal", [200, 20, 90]],
        },
        "stat_requests": [
            {"id": 1, "type": "Route", "from": "A & <", "to": "C"},
            {"id": 2, "type": "Route", "from": "C", "to": "A & <"},
            {"id": 3, "type": "Route", "from": "C", "to": "Unused"},
            {"id": 4, "type": "Map"},
        ],
    }


def protobuf_class():
    # Official Python runtime provides an independent decoder/encoder. Build the
    # tiny published schema dynamically so no protoc executable is needed.
    from google.protobuf import descriptor_pb2, descriptor_pool, message_factory
    file = descriptor_pb2.FileDescriptorProto(name="contact.proto", package="PhoneBookSerialize", syntax="proto3")
    specifications = {
        "Date": [("year", 1, 5, 1, ""), ("month", 2, 5, 1, ""), ("day", 3, 5, 1, "")],
        "Contact": [("name", 1, 9, 1, ""), ("birthday", 2, 11, 1, ".PhoneBookSerialize.Date"),
                    ("phone_number", 3, 9, 3, "")],
        "ContactList": [("contact", 1, 11, 3, ".PhoneBookSerialize.Contact")],
    }
    for name, fields in specifications.items():
        message = file.message_type.add(name=name)
        for field_name, number, kind, label, type_name in fields:
            field = message.field.add(name=field_name, number=number, type=kind, label=label)
            if type_name:
                field.type_name = type_name
    pool = descriptor_pool.DescriptorPool()
    pool.Add(file)
    return message_factory.GetMessageClass(pool.FindMessageTypeByName("PhoneBookSerialize.ContactList"))


def check(binary, official_protobuf=False):
    for payload, expected in [(b"9223372036854775807 1", b"Overflow!\n"),
                              (b"-9223372036854775808 -1", b"Overflow!\n"),
                              (b"-9223372036854775808 9223372036854775807", b"-1\n")]:
        assert run(binary, "sum", payload, raw=True) == expected
    data = transport_fixture()
    for projection in "GHIJK":
        responses = run(binary, "transport", data, projection)
        assert responses[0]["total_time"] == 5 and abs(responses[1]["total_time"] - 5.7) < 1e-9
        assert responses[0]["items"] == [{"type": "Wait", "stop_name": "A & <", "time": 2},
                                          {"type": "Bus", "bus": "X", "time": 3, "span_count": 2}]
        assert responses[2]["error_message"] == "not found"
        document = ET.fromstring(responses[3]["map"])
        tags = [child.tag.split("}")[-1] for child in document]
        assert tags.count("circle") == 4 and tags.count("polyline") == 1
        assert tags.count("text") == (8 if projection == "G" else 12)
        assert list(document)[0].attrib["points"].count(" ") == 4
        labels = [c.text for c in document if c.tag.endswith("text")]
        assert labels.count("A & <") == 2
    # Layer order, repetition and an empty layer list are part of the contract.
    data["render_settings"]["layers"] = ["stop_points", "bus_lines", "stop_points"]
    doc = ET.fromstring(run(binary, "transport", data, "I")[3]["map"])
    assert [c.tag.split("}")[-1] for c in doc] == ["circle"]*4+["polyline"]+["circle"]*4
    data["render_settings"]["layers"] = []
    assert len(ET.fromstring(run(binary, "transport", data, "I")[3]["map"])) == 0
    bad = copy.deepcopy(data); bad["routing_settings"]["bus_velocity"] = 0
    run(binary, "transport", bad, valid=False)
    bad = copy.deepcopy(data); bad["base_requests"][0]["road_distances"]["B"] = 1e100
    run(binary, "transport", bad, valid=False)

    results = run(binary, "sheet", [
        {"op": "set", "cell": "A1", "text": "42"},
        {"op": "set", "cell": "B2", "text": "=A1+8"},
        {"op": "get", "cell": "B2"},
        {"op": "insert_cols", "first": 0},
        {"op": "get", "cell": "C2"},
        {"op": "delete_rows", "first": 0},
        {"op": "get", "cell": "C1"},
    ])
    assert results[0]["value"] == results[1]["value"] == 50
    assert results[1]["text"] == "=B1+8" and results[1]["references"] == ["B1"]
    assert results[2]["error"] == "#REF!" and results[2]["references"] == []
    run(binary, "sheet", [{"op": "set", "cell": "A1", "text": "=A1"}], valid=False)

    records = [{"name": "Zoë", "phones": ["+123", ""]},
               {"name": "Анна", "phones": ["7"], "birthday": {"year": -1, "month": 2, "day": 29}},
               {"name": "Ann", "phones": [], "birthday": {"year": 2000, "month": 1, "day": 1}}]
    encoded = run(binary, "phonebook-encode", records, raw=True)
    decoded = run(binary, "phonebook-decode", encoded)
    assert decoded == sorted(records, key=lambda r: r["name"])
    assert run(binary, "phonebook-decode", encoded, "Ан") == [records[1]]
    # Fixed wire golden: ContactList(contact=[Contact(name="A", phone_number=["7"])])
    golden = bytes.fromhex("0a060a01411a0137")
    assert run(binary, "phonebook-encode", [{"name": "A", "phones": ["7"]}], raw=True) == golden
    assert run(binary, "phonebook-decode", golden+b"\xa0\x06\x01") == [{"name": "A", "phones": ["7"]}]
    for malformed in [golden[:-1], b"\x00", b"\x0a\xff", b"\xff"*11]:
        run(binary, "phonebook-decode", malformed, valid=False)
    if official_protobuf:
        cls = protobuf_class(); message = cls.FromString(encoded)
        assert [c.name for c in message.contact] == [r["name"] for r in decoded]
        assert message.contact[2].birthday.year == -1
        message.contact[0].phone_number.append("from official runtime")
        decoded[0]["phones"].append("from official runtime")
        assert run(binary, "phonebook-decode", message.SerializeToString()) == decoded
    print("Black CLI: routes, SVG/XML G–K, layers, sheet edits, protobuf golden/malformed PASS"
          + ("; official protobuf bidirectional interoperability PASS" if official_protobuf else ""))
