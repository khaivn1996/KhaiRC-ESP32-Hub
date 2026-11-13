# Claude CLI Directives — KhaiRC-ESP32-Hub

@AGENTS.md

## Quy ước điều hướng Skill trên Claude CLI
- Khi người dùng gõ `$architect`, `$implement`, `$reviewer`, `$debugger`, `$docs` (hoặc `/<tên_skill>`), tuân thủ nghiêm ngặt quy trình của vai đó. Skill đã tự nạp qua `.claude/skills` (symlink tới `.agents/skills`): nếu nội dung SKILL.md đã có trong ngữ cảnh thì KHÔNG đọc lại; chỉ đọc `.agents/skills/<tên_skill>/SKILL.md` khi chưa được nạp.
- Không dùng tool `Agent`/`Task` (subagent, Explore, Plan agent), kể cả khi plan mode hoặc skill khác gợi ý; luật đầy đủ ở `AGENTS.md` §7. Khi plan mode bắt dùng Explore agent, tự đọc file trực tiếp bằng Read/Grep/Glob.