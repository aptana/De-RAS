// De-RAS - core/Messages.h
// Bilingual (IDN/ENG) diagnostic messages shared by the I/O layer, the
// Model Doctor, the solver and the UI. A message carries a stable code and
// parameters; text is produced from the catalogue in the language chosen by
// the user, so the same event reads correctly in both languages.
#pragma once

#include <string>
#include <vector>

namespace deras {

enum class Language { ID, EN };
enum class Severity { Info, Warning, Error };

struct Message {
    Severity severity = Severity::Info;
    std::string code;                 // e.g. "XS_TOO_FEW_POINTS"
    std::vector<std::string> args;    // substituted into {0}, {1}, ...
    std::string location;             // e.g. "River/Reach RS 1000" (language neutral)

    std::string text(Language lang) const;  // "[location] body"
    std::string body(Language lang) const;  // the message without its location
};

using MessageList = std::vector<Message>;

Message makeMsg(Severity s, std::string code, std::vector<std::string> args = {},
                std::string location = {});

const char* severityLabel(Severity s, Language lang);

// Look up a catalogue string by code; returns the code itself if unknown.
std::string catalogText(const std::string& code, Language lang);

// How to fix the problem a message reports (Model Doctor pop-up, review R4 of
// build 0.2.0). Empty when the catalogue has no hint for the code.
std::string fixHint(const std::string& code, Language lang);

bool hasErrors(const MessageList& msgs);

// Format a double with a fixed number of decimals (locale independent).
std::string fmt(double x, int decimals = 3);

}  // namespace deras
