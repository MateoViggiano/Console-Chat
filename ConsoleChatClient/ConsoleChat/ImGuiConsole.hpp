#pragma once

#include <imgui.h>

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <cfloat>

class ImGuiConsole
{
public:
    enum class MessageType
    {
        Message,
        Command,
        File
    };

    struct SubmittedMessage
    {
        std::string targetUser;
        MessageType type;
        std::string text;
    };

public:
    ImGuiConsole()
    {
        outputBuffer.push_back('\0');

        targetUserBuffer.fill('\0');
        inputBuffer.fill('\0');
    }

    void Print(const std::string& text)
    {
        if (!outputBuffer.empty())
            outputBuffer.pop_back();

        outputBuffer.insert(outputBuffer.end(), text.begin(), text.end());
        outputBuffer.push_back('\n');
        outputBuffer.push_back('\0');

        scrollToBottom = true;
    }

    std::optional<SubmittedMessage> PollMessage()
    {
        if (!submittedMessage.has_value())
            return std::nullopt;

        auto result = submittedMessage;
        submittedMessage.reset();
        return result;
    }

    void Draw(const char* title = "Console")
    {
        ImGui::Begin(title);

        DrawOutput();
        DrawInputArea();

        ImGui::End();
    }

    void Clear()
    {
        outputBuffer.clear();
        outputBuffer.push_back('\0');
        scrollToBottom = true;
    }

    static const char* MessageTypeToString(MessageType type)
    {
        switch (type)
        {
        case MessageType::Message:
            return "Message";

        case MessageType::Command:
            return "Command";

        case MessageType::File:
            return "File";

        default:
            return "Unknown";
        }
    }

private:
    void DrawOutput()
    {
        const float inputAreaHeight =
            ImGui::GetFrameHeightWithSpacing() * 2.0f +
            ImGui::GetStyle().ItemSpacing.y;

        const ImVec2 outputSize(-FLT_MIN, -inputAreaHeight);

        const ImGuiInputTextFlags flags =
            ImGuiInputTextFlags_ReadOnly |
            ImGuiInputTextFlags_NoUndoRedo;

        ImGui::InputTextMultiline(
            "##ConsoleOutput",
            outputBuffer.data(),
            outputBuffer.size(),
            outputSize,
            flags
        );

        if (scrollToBottom && !ImGui::IsItemActive())
        {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;
        }
    }

    void DrawInputArea()
    {
        ImGui::Separator();

        DrawSmallFields();
        DrawMainInput();
    }

    void DrawSmallFields()
    {
        ImGui::TextUnformatted("Usuario:");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText(
            "##TargetUserInput",
            targetUserBuffer.data(),
            targetUserBuffer.size()
        );

        ImGui::SameLine();

        ImGui::TextUnformatted("Tipo:");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo(
            "##MessageTypeCombo",
            &selectedMessageTypeIndex,
            messageTypeNames,
            IM_ARRAYSIZE(messageTypeNames)
        );
    }

    void DrawMainInput()
    {
        ImGui::SetNextItemWidth(-FLT_MIN);

        const ImGuiInputTextFlags flags =
            ImGuiInputTextFlags_EnterReturnsTrue;

        const bool enterPressed = ImGui::InputText(
            "##MainMessageInput",
            inputBuffer.data(),
            inputBuffer.size(),
            flags
        );

        if (enterPressed)
        {
            std::string text = inputBuffer.data();

            if (!text.empty())
            {
                submittedMessage = SubmittedMessage
                {
                    std::string(targetUserBuffer.data()),
                    IndexToMessageType(selectedMessageTypeIndex),
                    text
                };

                // Se borra SOLO el mensaje principal.
                // Usuario y tipo quedan igual.
                inputBuffer.fill('\0');

                focusInputNextFrame = true;
            }
        }

        if (focusInputNextFrame)
        {
            ImGui::SetKeyboardFocusHere(-1);
            focusInputNextFrame = false;
        }
    }

    static MessageType IndexToMessageType(int index)
    {
        switch (index)
        {
        case 0:
            return MessageType::Message;

        case 1:
            return MessageType::Command;

        case 2:
            return MessageType::File;

        default:
            return MessageType::Message;
        }
    }

private:
    std::vector<char> outputBuffer;

    std::array<char, 64> targetUserBuffer;
    std::array<char, 1024> inputBuffer;

    static constexpr const char* messageTypeNames[] =
    {
        "Message",
        "Command",
        "File"
    };

    int selectedMessageTypeIndex = 0;

    std::optional<SubmittedMessage> submittedMessage;

    bool scrollToBottom = false;
    bool focusInputNextFrame = false;
};