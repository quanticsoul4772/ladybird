# Mermaid Diagram Reference

Quick reference for Mermaid diagram syntax and patterns.

## Diagram Types

### 1. Flowchart

```mermaid
flowchart TD
    A[Start] --> B{Decision}
    B -->|Yes| C[Process 1]
    B -->|No| D[Process 2]
    C --> E[End]
    D --> E
```

**Syntax**:
```
flowchart TD      # Top-Down
flowchart LR      # Left-Right
flowchart BT      # Bottom-Top
flowchart RL      # Right-Left
```

**Node Shapes**:
```
A[Rectangle]
B(Rounded)
C([Stadium])
D[[Subroutine]]
E[(Database)]
F((Circle))
G>Asymmetric]
H{Diamond}
I{{Hexagon}}
J[/Parallelogram/]
K[\Parallelogram\]
L[/Trapezoid\]
M[\Trapezoid/]
```

**Arrows**:
```
A --> B           # Solid arrow
A --- B           # Solid line
A -.-> B          # Dotted arrow
A ==> B           # Thick arrow
A <--> B          # Bidirectional
A --text--> B     # Labeled arrow
```

### 2. Sequence Diagram

```mermaid
sequenceDiagram
    participant A as Alice
    participant B as Bob

    A->>B: Hello Bob!
    activate B
    B-->>A: Hi Alice!
    deactivate B

    Note over A,B: This is a note
```

**Syntax**:
```
participant Name as Alias
actor Name              # Actor icon
participant Name        # Box icon

A->>B: Message          # Solid arrow
A-->>B: Dashed          # Dashed arrow
A-xB: Cross             # Cross end
A-)B: Open arrow        # Open arrow

activate A
deactivate A

Note left of A: Note
Note right of B: Note
Note over A,B: Note spanning

loop Loop text
    A->>B: Message
end

alt Alternative
    A->>B: Option 1
else
    A->>B: Option 2
end

opt Optional
    A->>B: Maybe
end

par Parallel
    A->>B: Task 1
and
    A->>C: Task 2
end
```

### 3. Class Diagram

```mermaid
classDiagram
    class Animal {
        +String name
        +int age
        +make_sound() void
    }

    class Dog {
        +String breed
        +bark() void
    }

    Animal <|-- Dog
```

**Syntax**:
```
class ClassName {
    +Public member
    -Private member
    #Protected member
    ~Package member

    +method() ReturnType
    method(param) ReturnType$  # Static
    method() ReturnType*       # Abstract
}

# Relationships
A <|-- B      # Inheritance
A *-- B       # Composition
A o-- B       # Aggregation
A --> B       # Association
A -- B        # Link
A ..> B       # Dependency
A ..|> B      # Realization

# Cardinality
A "1" --> "0..*" B
```

### 4. State Diagram

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Processing: start()
    Processing --> Complete: success
    Processing --> Error: failure
    Error --> Idle: reset()
    Complete --> [*]
```

**Syntax**:
```
[*] --> State1          # Initial state
State1 --> [*]          # Final state
State1 --> State2: event

state State1 {
    [*] --> SubState1
    SubState1 --> SubState2
}

note right of State1
    Important note
end note
```

### 5. Entity Relationship Diagram

```mermaid
erDiagram
    CUSTOMER ||--o{ ORDER : places
    ORDER ||--|{ LINE-ITEM : contains
    CUSTOMER {
        string name
        string email
    }
    ORDER {
        int id
        date created
    }
```

**Syntax**:
```
ENTITY1 ||--o| ENTITY2 : "relationship"

# Cardinality
||--||  # One to one
}|--||  # Zero or more to one
||--o|  # One to zero or one
}|--o|  # Zero or more to zero or one
```

### 6. Gantt Chart

```mermaid
gantt
    title Project Schedule
    dateFormat YYYY-MM-DD

    section Planning
    Requirements    :a1, 2025-01-01, 30d
    Design         :a2, after a1, 20d

    section Development
    Implementation :a3, after a2, 60d
    Testing       :a4, after a3, 30d
```

### 7. Pie Chart

```mermaid
pie
    title Distribution
    "Category A" : 45
    "Category B" : 30
    "Category C" : 25
```

### 8. Git Graph

```mermaid
gitGraph
    commit
    branch develop
    checkout develop
    commit
    checkout main
    merge develop
    commit
```

## Styling

### Theme Selection

```mermaid
%%{init: {'theme':'dark'}}%%
graph TD
    A --> B
```

Themes: `default`, `dark`, `forest`, `neutral`

### Custom Styles

```mermaid
graph TD
    A[Node A]
    B[Node B]

    style A fill:#f9f,stroke:#333,stroke-width:4px
    style B fill:#bbf,stroke:#333

    classDef important fill:#f96,stroke:#333,stroke-width:4px
    class A important
```

### Subgraphs

```mermaid
graph TB
    subgraph "Subgraph 1"
        A[Node A]
        B[Node B]
    end

    subgraph "Subgraph 2"
        C[Node C]
        D[Node D]
    end

    A --> C
```

## Advanced Features

### Callbacks (HTML Only)

```mermaid
graph TD
    A[Click me]
    click A callback "Tooltip text"
    click A "https://example.com" "Link tooltip"
```

### Comments

```mermaid
%% This is a comment
graph TD
    A --> B  %% This is also a comment
```

### Configuration

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'primaryColor': '#BB2528',
    'primaryTextColor': '#fff',
    'primaryBorderColor': '#7C0000',
    'lineColor': '#F8B229',
    'secondaryColor': '#006100',
    'tertiaryColor': '#fff'
  }
}}%%
graph TD
    A-->B
```

## Best Practices

### 1. Keep It Simple

```mermaid
%% Good: Simple and clear
flowchart TD
    A[User] --> B[System]
    B --> C[Database]

%% Bad: Too complex for one diagram
flowchart TD
    A[User] --> B[Auth Service]
    B --> C[User Service]
    C --> D[Database]
    B --> E[Cache]
    C --> F[Message Queue]
    F --> G[Worker 1]
    F --> H[Worker 2]
    %% ... 20 more nodes
```

### 2. Use Descriptive Labels

```mermaid
%% Good: Descriptive
flowchart TD
    User[Web Browser] --> Server[Application Server]
    Server --> DB[(PostgreSQL Database)]

%% Bad: Generic
flowchart TD
    A --> B
    B --> C
```

### 3. Group Related Components

```mermaid
flowchart TD
    subgraph "Frontend"
        UI[User Interface]
        Router[Router]
    end

    subgraph "Backend"
        API[API Gateway]
        Service[Business Logic]
    end

    UI --> Router --> API --> Service
```

### 4. Use Consistent Styling

```mermaid
%%{init: {'theme':'base'}}%%
graph TB
    classDef process fill:#e1f5ff,stroke:#0066cc
    classDef data fill:#fff4e1,stroke:#ff9800
    classDef external fill:#e8f5e8,stroke:#4caf50

    A[Process 1]:::process
    B[(Database)]:::data
    C[External API]:::external
```

## Mermaid CLI Usage

### Installation

```bash
npm install -g @mermaid-js/mermaid-cli
```

### Generate Images

```bash
# PNG output
mmdc -i diagram.mmd -o diagram.png

# SVG output
mmdc -i diagram.mmd -o diagram.svg

# PDF output
mmdc -i diagram.mmd -o diagram.pdf

# Custom dimensions
mmdc -i diagram.mmd -o diagram.png -w 1920 -H 1080

# Dark theme
mmdc -i diagram.mmd -o diagram.png -t dark

# Custom background
mmdc -i diagram.mmd -o diagram.png -b transparent
```

### Batch Processing

```bash
# Process all .mmd files
for file in *.mmd; do
    mmdc -i "$file" -o "${file%.mmd}.svg"
done
```

## Integration

### Markdown

````markdown
```mermaid
graph TD
    A --> B
```
````

### HTML

```html
<div class="mermaid">
graph TD
    A --> B
</div>

<script src="https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js"></script>
<script>mermaid.initialize({startOnLoad:true});</script>
```

### Programmatic

```javascript
const mermaid = require('mermaid');

mermaid.initialize({ startOnLoad: true });

// Or render specific element
mermaid.render('graphDiv', 'graph TD\nA-->B', (svgCode) => {
    document.getElementById('output').innerHTML = svgCode;
});
```

## Common Patterns for Ladybird

### Multi-Process Architecture

```mermaid
graph TB
    subgraph "UI Process"
        Browser[Browser UI]
    end

    subgraph "Per-Tab Processes"
        WC1[WebContent #1]
        WC2[WebContent #2]
    end

    Browser --> WC1
    Browser --> WC2
```

### Request Flow

```mermaid
sequenceDiagram
    UI->>WebContent: Navigate
    WebContent->>RequestServer: HTTP Request
    RequestServer->>Network: TCP/TLS
    Network-->>RequestServer: Response
    RequestServer-->>WebContent: Data
    WebContent->>WebContent: Parse & Render
    WebContent-->>UI: Update Display
```

### State Machines

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Loading: start_load()
    Loading --> Parsing: response_received()
    Parsing --> Rendering: dom_ready()
    Rendering --> Complete: paint_complete()
    Complete --> [*]
```

## Resources

- **Live Editor**: https://mermaid.live/
- **Documentation**: https://mermaid.js.org/
- **GitHub**: https://github.com/mermaid-js/mermaid
- **CLI Tool**: https://github.com/mermaid-js/mermaid-cli
