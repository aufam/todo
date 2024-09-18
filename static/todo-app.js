let token = null;
let g_username = null;

// Handle Signup
async function signup() {
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;

    const response = await fetch('/user/signup', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username, password })
    });

    const result = await response.text();
    if (response.ok) {
        token = result;
        g_username = username;
        document.getElementById('auth-status').textContent = 'Signup successful, logged in!';
        document.getElementById('auth-section').style.display = 'none';
        document.getElementById('todo-section').style.display = 'block';
        loadTodos();
    } else {
        document.getElementById('auth-status').textContent = 
            `Signup failed (${response.statusText}): ${result}`;
    }
}

// Handle Login
async function login() {
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;

    const response = await fetch('/user/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ username, password })
    });

    const result = await response.text();
    if (response.ok) {
        token = result;
        g_username = username;
        document.getElementById('auth-status').textContent = 'Login successful!';
        document.getElementById('auth-section').style.display = 'none';
        document.getElementById('todo-section').style.display = 'block';
        loadTodos();
    } else {
        document.getElementById('auth-status').textContent = 
            `Login failed (${response.statusText}): ${result}`;
    }
}

// Load Todos
async function loadTodos() {
    const response = await fetch('/todos', {
        headers: { 'Authentication': `Bearer ${token}` }
    });
    const headerTitle = document.getElementById('header-title');
    headerTitle.textContent = `${g_username}'s todo list`;

    const todos = await response.json();
    const todoList = document.getElementById('todo-list');
    todoList.innerHTML = '';

    if (response.ok) {
        todos.forEach(todo => {
            const todoItem = document.createElement('div');
            todoItem.className = `todo-item ${todo.is_done ? 'completed' : ''}`;
            todoItem.innerHTML = `
                <h3>${todo.task}</h3>
                <h8>Created at: ${todo.created_at}</h8>
                <button onclick="toggleTodoStatus(${todo.id}, ${todo.is_done})">${todo.is_done ? 'Undo' : 'Complete'}</button>
                <button onclick="deleteTodo(${todo.id})">Delete</button>
            `;
            todoList.appendChild(todoItem);
        });
    }
}

// Create Todo
async function createTodo() {
    const task = document.getElementById('new-todo').value;

    const response = await fetch('/todo', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Authentication': `Bearer ${token}`
        },
        body: JSON.stringify({ task })
    });

    if (response.ok) {
        loadTodos();
    }
}

// Toggle Todo Status
async function toggleTodoStatus(id, currentStatus) {
    const response = await fetch(`/todo?id=${id}`, {
        method: 'PUT',
        headers: {
            'Content-Type': 'application/json',
            'Authentication': `Bearer ${token}`
        },
        body: JSON.stringify({ is_done: !currentStatus })
    });

    if (response.ok) {
        loadTodos();
    }
}

// Delete Todo
async function deleteTodo(id) {
    const response = await fetch(`/todo?id=${id}`, {
        method: 'DELETE',
        headers: {
            'Authentication': `Bearer ${token}`
        }
    });

    if (response.ok) {
        loadTodos();
    }
}

// Logout
function logout() {
    token = null;
    document.getElementById('auth-section').style.display = 'block';
    document.getElementById('todo-section').style.display = 'none';
}

