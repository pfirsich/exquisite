#pragma once

#include <deque>
#include <memory>

template <typename Action>
class ActionStack {
public:
    Action& getTop()
    {
        return actions_.back();
    }

    template <typename... Args>
    void perform(Args&&... args)
    {
        if (undidCount_ > 0) {
            actions_.resize(actions_.size() - undidCount_);
            undidCount_ = 0;
        }

        actions_.emplace_back(std::forward<Args>(args)...).perform();
    }

    bool undo()
    {
        if (actions_.empty() || undidCount_ == actions_.size())
            return false;

        auto& action = actions_[actions_.size() - undidCount_ - 1];
        action.undo();
        undidCount_++;
        return true;
    }

    bool redo()
    {
        if (undidCount_ == 0)
            return false;
        auto& action = actions_[actions_.size() - undidCount_];
        action.perform();
        undidCount_--;
        return true;
    }

    void clear()
    {
        actions_.clear();
    }

private:
    size_t undidCount_ = 0;
    std::deque<Action> actions_;
};
