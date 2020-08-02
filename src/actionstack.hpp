#pragma once

#include <deque>
#include <memory>

template <typename Action>
class ActionStack {
public:
    template <typename... Args>
    void perform(Args&&... args)
    {
        popRedoable();
        actions_.emplace_back(std::forward<Args>(args)...).perform();
    }

    bool undo()
    {
        if (actions_.empty() || undoneCount_ == actions_.size())
            return false;

        auto& action = actions_[actions_.size() - undoneCount_ - 1];
        action.undo();
        undoneCount_++;
        return true;
    }

    bool redo()
    {
        if (undoneCount_ == 0)
            return false;
        auto& action = actions_[actions_.size() - undoneCount_];
        action.perform();
        undoneCount_--;
        return true;
    }

    void popRedoable()
    {
        if (undoneCount_ > 0) {
            actions_.resize(actions_.size() - undoneCount_);
            undoneCount_ = 0;
        }
    }

    void clear()
    {
        actions_.clear();
    }

    Action& getTop()
    {
        return actions_[actions_.size() - 1 - undoneCount_];
    }

    size_t getSize() const
    {
        return actions_.size() - undoneCount_;
    }

    size_t getUndoneCount() const
    {
        return undoneCount_;
    }

private:
    size_t undoneCount_ = 0;
    std::deque<Action> actions_;
};
