from fastapi import APIRouter
from pydantic import BaseModel, Field

from admin_service.app.ai.service import ai_worker

router = APIRouter(prefix="/ai", tags=["ai"])


class ModerateRequest(BaseModel):
    text: str = Field(default="", min_length=0, max_length=4096)


class ChatbotRequest(BaseModel):
    text: str = Field(default="", min_length=0, max_length=4096)


class SummaryRequest(BaseModel):
    messages: list[str] = Field(default_factory=list)


@router.post("/moderate")
async def moderate(req: ModerateRequest) -> dict:
    return await ai_worker.submit("moderate", {"text": req.text})


@router.post("/chatbot")
async def chatbot(req: ChatbotRequest) -> dict:
    return await ai_worker.submit("chatbot", {"text": req.text})


@router.post("/summary")
async def summary(req: SummaryRequest) -> dict:
    return await ai_worker.submit("summary", {"messages": req.messages})
