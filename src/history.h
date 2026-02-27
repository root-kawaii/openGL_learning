#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <algorithm>

class GameObject; // forward decl

// Snapshot of every field that editor operations can change
struct TransformState
{
    glm::vec3 position{0};
    glm::vec3 rotation{0}; // radians
    glm::vec3 scale{1};
    glm::vec3 color{1};
};

enum class HistoryCmdType
{
    TRANSFORM,  // position / rotation / scale / color changed
    ADD_OBJECT, // object was added  (undo = remove, redo = re-add)
};

struct HistoryCommand
{
    HistoryCmdType type = HistoryCmdType::TRANSFORM;

    // TRANSFORM
    uint32_t       objectId = 0;
    TransformState before;
    TransformState after;

    // ADD_OBJECT — keep ptr alive so undo can remove and redo can re-insert
    std::shared_ptr<GameObject> addedObject;
};

// ------------------------------------------------------------------
// Thin stack manager — knows nothing about the scene, just commands.
// ------------------------------------------------------------------
class HistoryManager
{
public:
    static constexpr int MAX_STACK = 64;

    void push(HistoryCommand cmd)
    {
        // Any new action wipes the redo future
        redoStack.clear();

        undoStack.push_back(std::move(cmd));
        if ((int)undoStack.size() > MAX_STACK)
            undoStack.erase(undoStack.begin());
    }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    // Pop from undo stack → caller applies it → caller pushes inverse to redo
    HistoryCommand popUndo()
    {
        HistoryCommand cmd = std::move(undoStack.back());
        undoStack.pop_back();
        return cmd;
    }

    HistoryCommand popRedo()
    {
        HistoryCommand cmd = std::move(redoStack.back());
        redoStack.pop_back();
        return cmd;
    }

    void pushRedo(HistoryCommand cmd) { redoStack.push_back(std::move(cmd)); }
    void pushUndo(HistoryCommand cmd) { undoStack.push_back(std::move(cmd)); }

private:
    std::vector<HistoryCommand> undoStack;
    std::vector<HistoryCommand> redoStack;
};
