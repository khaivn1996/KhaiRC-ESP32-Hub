# Claude CLI Directives — KhaiRC-ESP32-Hub

@AGENTS.md

## Quy ước điều hướng Skill trên Claude CLI
- Khi người dùng gõ `$architect`, `$implement`, `$reviewer`, `$debugger`, hoặc `$docs` ở đầu câu lệnh, hãy đọc ngay file `.agents/skills/<tên_skill>/SKILL.md` tương ứng và tuân thủ nghiêm ngặt quy trình của vai đó.
- Tuyệt đối không sử dụng `Task` tool (Subagent/Explore agent) trong mọi trường hợp để bảo toàn hạn mức token.