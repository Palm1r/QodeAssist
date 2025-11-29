/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

namespace QodeAssist::OpenAIResponses {

/*
 * REFERENCE: Item Types in List Input Items Response
 * ===================================================
 * 
 * The `data` array in ListInputItemsResponse can contain various item types.
 * This file serves as a reference for all possible item types.
 * 
 * EXISTING TYPES (already implemented):
 * -------------------------------------
 * - MessageOutput (in ResponseObject.hpp)
 * - FunctionCall (in ResponseObject.hpp)
 * - ReasoningOutput (in ResponseObject.hpp)
 * - FileSearchCall (in ResponseObject.hpp)
 * - CodeInterpreterCall (in ResponseObject.hpp)
 * - Message (in ModelRequest.hpp) - for input messages
 * 
 * ADDITIONAL TYPES (to be implemented if needed):
 * -----------------------------------------------
 * 
 * 1. Computer Tool Call (computer_call)
 *    - Computer use tool for UI automation
 *    - Properties: action, call_id, id, pending_safety_checks, status, type
 *    - Actions: click, double_click, drag, keypress, move, screenshot, scroll, type, wait
 * 
 * 2. Computer Tool Call Output (computer_call_output)
 *    - Output from computer tool
 *    - Properties: call_id, id, output, type, acknowledged_safety_checks, status
 * 
 * 3. Web Search Tool Call (web_search_call)
 *    - Web search results
 *    - Properties: action, id, status, type
 *    - Actions: search, open_page, find
 * 
 * 4. Image Generation Call (image_generation_call)
 *    - AI image generation request
 *    - Properties: id, result (base64), status, type
 * 
 * 5. Local Shell Call (local_shell_call)
 *    - Execute shell commands locally
 *    - Properties: action (exec), call_id, id, status, type
 *    - Action properties: command, env, timeout_ms, user, working_directory
 * 
 * 6. Local Shell Call Output (local_shell_call_output)
 *    - Output from local shell execution
 *    - Properties: id, output (JSON string), type, status
 * 
 * 7. Shell Tool Call (shell_call)
 *    - Managed shell environment execution
 *    - Properties: action, call_id, id, status, type, created_by
 * 
 * 8. Shell Call Output (shell_call_output)
 *    - Output from shell tool
 *    - Properties: call_id, id, max_output_length, output (array), type, created_by
 *    - Output chunks: outcome (exit/timeout), stderr, stdout
 * 
 * 9. Apply Patch Tool Call (apply_patch_call)
 *    - File diff operations
 *    - Properties: call_id, id, operation, status, type, created_by
 *    - Operations: create_file, delete_file, update_file
 * 
 * 10. Apply Patch Tool Call Output (apply_patch_call_output)
 *     - Output from patch operations
 *     - Properties: call_id, id, status, type, created_by, output
 * 
 * 11. MCP List Tools (mcp_list_tools)
 *     - List of tools from MCP server
 *     - Properties: id, server_label, tools (array), type, error
 * 
 * 12. MCP Approval Request (mcp_approval_request)
 *     - Request for human approval
 *     - Properties: arguments, id, name, server_label, type
 * 
 * 13. MCP Approval Response (mcp_approval_response)
 *     - Response to approval request
 *     - Properties: approval_request_id, approve (bool), id, type, reason
 * 
 * 14. MCP Tool Call (mcp_call)
 *     - Tool invocation on MCP server
 *     - Properties: arguments, id, name, server_label, type
 *     - Optional: approval_request_id, error, output, status
 * 
 * 15. Custom Tool Call (custom_tool_call)
 *     - User-defined tool call
 *     - Properties: call_id, input, name, type, id
 * 
 * 16. Custom Tool Call Output (custom_tool_call_output)
 *     - Output from custom tool
 *     - Properties: call_id, output (string or array), type, id
 * 
 * 17. Item Reference (item_reference)
 *     - Internal reference to another item
 *     - Properties: id, type
 * 
 * USAGE:
 * ------
 * When parsing ListInputItemsResponse.data array:
 * 1. Check item["type"] field
 * 2. Use appropriate parser based on type
 * 3. For existing types, use ResponseObject.hpp or ModelRequest.hpp
 * 4. For additional types, implement parsers as needed
 * 
 * EXAMPLE:
 * --------
 * for (const auto &itemValue : response.data) {
 *     const QJsonObject itemObj = itemValue.toObject();
 *     const QString type = itemObj["type"].toString();
 *     
 *     if (type == "message") {
 *         // Use MessageOutput or Message
 *     } else if (type == "function_call") {
 *         // Use FunctionCall
 *     } else if (type == "computer_call") {
 *         // Implement ComputerCall parser
 *     }
 *     // ... handle other types
 * }
 */

} // namespace QodeAssist::OpenAIResponses

