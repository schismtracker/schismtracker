local _task_queue = {
    first = 0,
    last = -1,
}

function _push_task(task)
    assert(type(task) == "function", "task must be a function")
    _task_queue.last = _task_queue.last + 1
    _task_queue[_task_queue.last] = task
end

function _pop_task()
    if (_task_queue.first > _task_queue.last) then
        return nil
    end

    local task = _task_queue[_task_queue.first]
    _task_queue[_task_queue.first] = nil
    _task_queue.first = _task_queue.first + 1

    if (_task_queue.first > _task_queue.last) then
        _task_queue.first = 0
        _task_queue.last = -1
    end

    return task
end
