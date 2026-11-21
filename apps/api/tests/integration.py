import concurrent.futures
import http.cookiejar
import json
import os
import secrets
import urllib.error
import urllib.request

URL = os.getenv("FLIPKILN_BASE_URL", "http://127.0.0.1:8080").rstrip("/")
ADMIN_PASSWORD = os.getenv("FLIPKILN_ADMIN_PASSWORD", "flipkiln-local-demo")
DEMO_PASSWORD = os.getenv("FLIPKILN_DEMO_PASSWORD", "flipkiln-local-demo")
DEMO_TOKEN = os.getenv("FLIPKILN_DEMO_TOKEN", "flipkiln-local-demo-read-token")


class Browser:
    def __init__(self):
        self.opener = urllib.request.build_opener(
            urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar())
        )

    def call(self, method, path, body=None, *, status=200, token=None, headers=None):
        request_headers = dict(headers or {})
        if body is not None:
            request_headers.setdefault("Content-Type", "application/json")
        if token is not None:
            request_headers["Authorization"] = f"Bearer {token}"
        request = urllib.request.Request(
            f"{URL}{path}",
            data=None if body is None else json.dumps(body).encode(),
            headers=request_headers,
            method=method,
        )
        try:
            response = self.opener.open(request, timeout=15)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            text = response.read()
            result = (response.status, json.loads(text) if text else None, response.headers)
        allowed = (status,) if isinstance(status, int) else status
        assert result[0] in allowed, (method, path, allowed, result[0], result[1])
        return result

    def login(self, username, password):
        _, session, _ = self.call("POST", "/api/auth/login", {"username": username, "password": password})
        assert session == {"username": username}
        self.call("GET", "/api/auth/session")


def main():
    admin = Browser()
    anon = Browser()
    anon.call("GET", "/api/manage/projects", status=401)
    _, _, challenge = anon.call(
        "GET", "/api/client/projects/workshop/environments/development/snapshot", status=401
    )
    assert challenge["WWW-Authenticate"] == "Bearer"
    workshop = "/api/client/projects/workshop/environments/development/snapshot"
    _, fixture, mime = anon.call("GET", workshop, headers={"Authorization": f"bEaReR {DEMO_TOKEN}"})
    assert len(mime.get_all("Content-Type")) == 1
    assert mime["Content-Type"].split(";")[0] == "application/json"
    assert any(entry["key"] == "checkout.fast_lane" for entry in fixture["entries"])
    anon.call(
        "GET",
        workshop.replace("development", "production"),
        status=401,
        token=DEMO_TOKEN,
    )
    admin.login("admin", ADMIN_PASSWORD)
    project_id = f"integration-{secrets.token_hex(4)}"
    projects = "/api/manage/projects"
    admin.call("POST", projects, {"id": project_id, "name": "HTTP integration"}, status=201)
    project = f"{projects}/{project_id}"
    development = f"{project}/environments/development"
    production = f"{project}/environments/production"
    client = f"/api/client/projects/{project_id}/environments/development"
    _, start, _ = admin.call("GET", f"{development}/state")
    assert start == {"draftVersion": 1, "entries": [], "active": {"revision": 1, "entries": []}}

    editor, viewer, publisher = Browser(), Browser(), Browser()
    for name, browser in (("editor", editor), ("viewer", viewer), ("publisher", publisher)):
        browser.login(name, DEMO_PASSWORD)
        browser.call("GET", f"{development}/state", status=404)
        admin.call("PUT", f"{project}/members/{name}", {"role": name})
        browser.call("GET", f"{development}/state")
    viewer.call("PUT", f"{development}/draft", {"draftVersion": 1, "entries": []}, status=403)
    editor.call(
        "POST",
        f"{development}/publish",
        {"draftVersion": 1, "note": "forbidden"},
        status=403,
        headers={"Idempotency-Key": "editor-must-not-publish"},
    )
    publisher.call("GET", f"{development}/tokens", status=403)

    _, created, _ = admin.call("POST", f"{development}/tokens", {"name": "SDK test"}, status=201)
    token = created["token"]
    _, token_list, _ = admin.call("GET", f"{development}/tokens")
    assert all("token" not in entry for entry in token_list["tokens"])
    _, initial, cached = anon.call("GET", f"{client}/snapshot", token=token)
    assert initial == {"revision": 1, "entries": []}
    anon.call(
        "GET",
        f"{client}/snapshot".replace("development", "production"),
        status=401,
        token=token,
        headers={"If-None-Match": "*"},
    )

    flag = {
        "key": "test.flag",
        "description": "HTTP integration flag",
        "type": "boolean",
        "defaultValue": True,
        "enabled": True,
        "rules": [
            {"id": "staff", "kind": "ids", "ids": ["chosen"], "value": False},
            {"id": "attribute", "kind": "attribute", "attribute": "plan", "equals": "pro", "value": True},
            {"id": "rollout", "kind": "rollout", "percentage": 100, "value": False},
        ],
    }
    config = {
        "key": "test.config",
        "description": "HTTP integration JSON",
        "type": "json",
        "defaultValue": {"maxItems": 0},
        "enabled": True,
        "rules": [],
        "schema": {
            "type": "object",
            "properties": {"maxItems": {"type": "integer", "minimum": 1}},
            "required": ["maxItems"],
            "additionalProperties": False,
        },
    }
    _, validation, _ = editor.call("POST", f"{development}/validate", {"entries": [flag, config]})
    assert not validation["valid"]
    assert validation["issues"][0]["path"] == "/entries/1/defaultValue/maxItems"
    editor.call("PUT", f"{development}/draft", {"draftVersion": 1, "entries": [flag, config]}, status=422)
    config["defaultValue"]["maxItems"] = 2
    entries = [flag, config]

    def save(draft):
        return editor.call(
            "PUT", f"{development}/draft", {"draftVersion": 1, "entries": draft}, status=(200, 409)
        )

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        saves = list(pool.map(save, (entries, entries)))
    assert sorted(result[0] for result in saves) == [200, 409]
    saved = next(result[1] for result in saves if result[0] == 200)
    assert saved["draftVersion"] == 2
    assert saved["active"] == initial
    _, _, unchanged = anon.call(
        "GET",
        f"{client}/snapshot",
        token=token,
        status=304,
        headers={"If-None-Match": f'"different", W/{cached["ETag"]}'},
    )
    assert unchanged["ETag"] == cached["ETag"]
    assert unchanged["Content-Type"] is None
    evaluate = {
        "keys": ["test.flag", "test.config", "missing.key"],
        "context": {"id": "chosen", "attributes": {"plan": "pro"}},
        "expectedTypes": {"test.config": "string"},
    }
    _, preview, _ = editor.call("POST", f"{development}/preview", {"entries": entries, **evaluate})
    assert preview["results"] == [
        {"key": "test.flag", "value": False, "reason": "rule_match", "ruleId": "staff"},
        {"key": "test.config", "reason": "type_mismatch"},
        {"key": "missing.key", "reason": "unknown_key"},
    ]

    publish = {"draftVersion": 2, "note": "Verify immutable releases"}
    operation = {"Idempotency-Key": "release-2"}

    def send_publish(_):
        return publisher.call("POST", f"{development}/publish", publish, headers=operation)[1]

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        releases = list(pool.map(send_publish, range(2)))
    assert releases == [{"revision": 2, "entries": entries}] * 2
    publisher.call(
        "POST",
        f"{development}/publish",
        publish,
        status=409,
        headers={"Idempotency-Key": "different-attempt"},
    )
    publisher.call(
        "POST", f"{development}/publish", {**publish, "note": "different"}, status=409, headers=operation
    )
    _, snapshot, next_tag = anon.call(
        "GET",
        f"{client}/snapshot",
        token=token,
        headers={"If-None-Match": cached["ETag"]},
    )
    assert snapshot == releases[0]
    assert cached["ETag"] != next_tag["ETag"]
    _, resolved, _ = anon.call("POST", f"{client}/evaluate", evaluate, token=token)
    assert resolved == {"revision": 2, "results": preview["results"]}
    _, defaulted, _ = anon.call(
        "POST", f"{client}/evaluate", {"keys": ["test.flag"], "context": {}}, token=token
    )
    assert defaulted["results"] == [{"key": "test.flag", "value": True, "reason": "default"}]
    _, rolled_out, _ = anon.call(
        "POST", f"{client}/evaluate", {"keys": ["test.flag"], "context": {"id": "unlisted"}}, token=token
    )
    assert rolled_out["results"] == [
        {"key": "test.flag", "value": False, "reason": "rule_match", "ruleId": "rollout"}
    ]

    _, archived, _ = editor.call("PUT", f"{development}/draft", {"draftVersion": 3, "entries": [config]})
    assert archived["active"] == releases[0]
    _, removed, _ = publisher.call(
        "POST",
        f"{development}/publish",
        {"draftVersion": 4, "note": "Archive the flag"},
        headers={"Idempotency-Key": "archive-flag"},
    )
    assert removed == {"revision": 3, "entries": [config]}
    assert anon.call("GET", f"{client}/snapshot", token=token)[1] == removed
    editor.call("PUT", f"{development}/draft", {"draftVersion": 5, "entries": entries})
    _, rollback, _ = publisher.call(
        "POST",
        f"{development}/rollback",
        {"revision": 1, "draftVersion": 6, "note": "Restore initial configuration"},
        headers={"Idempotency-Key": "restore-1"},
    )
    assert rollback == {"revision": 4, "entries": []}
    _, state, _ = admin.call("GET", f"{development}/state")
    assert state == {"draftVersion": 7, "entries": [], "active": rollback}
    assert send_publish(None) == releases[0]
    _, history, _ = admin.call("GET", f"{development}/releases")
    assert [(row["revision"], row["kind"], row["sourceRevision"]) for row in history["releases"]] == [
        (4, "rollback", 1),
        (3, "publish", None),
        (2, "publish", None),
        (1, "initial", None),
    ]
    _, original, _ = viewer.call("GET", f"{development}/releases/2")
    assert original["entries"] == entries
    assert original["actor"] == "publisher"
    assert original["note"] == publish["note"]
    _, untouched, _ = admin.call("GET", f"{production}/state")
    assert untouched == start

    changed_type = dict(flag, type="string", defaultValue="changed", rules=[])
    _, rejected, _ = editor.call("POST", f"{development}/validate", {"entries": [changed_type]})
    assert rejected["valid"] is False
    assert rejected["issues"][0]["path"] == "/entries/0/type"
    admin.call("DELETE", f"{development}/tokens/{created['id']}", status=204)
    anon.call("GET", f"{client}/snapshot", status=401, token=token, headers={"If-None-Match": "*"})
    admin.call("PUT", f"{project}/members/editor", {"role": None})
    editor.call("GET", f"{development}/state", status=404)
    _, _, logout = admin.call(
        "POST",
        "/api/auth/logout",
        {},
        status=204,
        headers={"Content-Type": "Application/JSON; charset=UTF-8"},
    )
    assert logout["Content-Type"] is None
    admin.call("GET", "/api/auth/session", status=401)
    print("HTTP integration passed: roles, concurrent writes, validation, publish, cache, rollout, rollback")


if __name__ == "__main__":
    main()
