from __future__ import annotations

import asyncio
from dataclasses import dataclass
from typing import Any

from admin_service.app.core.config import settings


@dataclass
class AITask:
    task_type: str
    payload: dict[str, Any]
    future: asyncio.Future


class AIWorker:
    def __init__(self) -> None:
        self._queue: asyncio.Queue[AITask] | None = None
        self._worker_task: asyncio.Task | None = None

    async def start(self) -> None:
        if self._worker_task is None:
            self._queue = asyncio.Queue()
            self._worker_task = asyncio.create_task(self._loop())

    async def stop(self) -> None:
        if self._worker_task is not None:
            self._worker_task.cancel()
            try:
                await self._worker_task
            except asyncio.CancelledError:
                pass
            self._worker_task = None
            self._queue = None

    async def submit(self, task_type: str, payload: dict[str, Any]) -> dict[str, Any]:
        # Keep the queue-based worker structure available for future expansion,
        # but execute inline by default so local runs and tests stay reliable.
        if task_type == "moderate":
            return self._moderate(payload.get("text", ""))
        if task_type == "chatbot":
            return self._chatbot(payload.get("text", ""))
        if task_type == "summary":
            return self._summary(payload.get("messages", []))
        return {"ok": False, "errmsg": "unsupported task type"}

    async def _loop(self) -> None:
        if self._queue is None:
            return

        while True:
            task = await self._queue.get()
            try:
                if task.task_type == "moderate":
                    result = self._moderate(task.payload.get("text", ""))
                elif task.task_type == "chatbot":
                    result = self._chatbot(task.payload.get("text", ""))
                elif task.task_type == "summary":
                    result = self._summary(task.payload.get("messages", []))
                else:
                    result = {"ok": False, "errmsg": "unsupported task type"}
                task.future.set_result(result)
            except Exception as exc:
                task.future.set_result({"ok": False, "errmsg": str(exc)})
            finally:
                self._queue.task_done()

    def _moderate(self, text: str) -> dict[str, Any]:
        normalized = text.lower()
        hit_words = [w for w in settings.sensitive_words if w in normalized]
        return {
            "ok": True,
            "action": "block" if hit_words else "allow",
            "hit_words": hit_words,
        }

    def _chatbot(self, text: str) -> dict[str, Any]:
        text = text.strip()
        if not text:
            return {"ok": True, "reply": "Please provide a message."}
        return {
            "ok": True,
            "reply": f"Bot: I received your message ({len(text)} chars).",
        }

    def _summary(self, messages: list[str]) -> dict[str, Any]:
        if not messages:
            return {"ok": True, "summary": "No messages."}
        first = messages[0][:48]
        last = messages[-1][:48]
        return {
            "ok": True,
            "summary": f"messages={len(messages)}, first='{first}', last='{last}'",
        }


ai_worker = AIWorker()
