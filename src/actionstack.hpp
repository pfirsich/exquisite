#pragma once

#include <deque>
#include <memory>

template <typename Action>
class ActionStack {
public:
    void perform(Action&& action, bool groupedWithPrev = false)
    {
        popRedoable();
        actions_.push_back(Element { std::move(action), groupedWithPrev });
        actions_.back().action.perform();
    }

    size_t undo()
    {
        size_t count = 0;
        bool undoNext = getSize() > 0;
        while (undoNext) {
            const auto& elem = actions_[actions_.size() - 1 - undoneCount_];
            elem.action.undo();
            undoneCount_++;
            count++;
            undoNext = getSize() > 0 && elem.groupedWithPrev;
        }
        return count;
    }

    size_t redo()
    {
        size_t count = 0;
        bool redoNext = undoneCount_ > 0;
        while (redoNext) {
            const auto& elem = actions_[actions_.size() - undoneCount_];
            elem.action.perform();
            undoneCount_--;
            count++;
            redoNext = undoneCount_ > 0 && actions_[actions_.size() - undoneCount_].groupedWithPrev;
        }
        return count;

        if (undoneCount_ == 0)
            return false;
        const auto& elem = actions_[actions_.size() - undoneCount_];
        elem.action.perform();
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

    const Action& getTop() const
    {
        return actions_[actions_.size() - 1 - undoneCount_].action;
    }

    Action& getTop()
    {
        return actions_[actions_.size() - 1 - undoneCount_].action;
    }

    size_t getSize() const
    {
        return actions_.size() - undoneCount_;
    }

private:
    struct Element {
        Action action;
        bool groupedWithPrev;
    };

    size_t undoneCount_ = 0;
    std::deque<Element> actions_;
};
