$$
\begin{align}
    [\text{Prog}] &\to [\text{Stmt}]^* \\
    [\text{Stmt}] &\to
    \begin{cases}
        \text{exit}([\text{Expr}]); \\
        \text{let}\space\text{ident} = [\text{Expr}]; \\
        \text{if([Expr])}[\text{Scope}]\\
        [\text{Scope}]
    \end{cases} \\
    [\text{Scope}] &\to  \{[\text{Stmt}]^*\}\\
    [\text{Expr}] &\to
    \begin{cases}
        [\text{Term}]   \\
        [\text{BinExpr}]
    \end{cases} \\
    [\text{BinExpr}] &\to
     \begin{cases}
        [\text{Expr}] / [\text{Expr}] & \text {prec} = 1 \\
        [\text{Expr}] * [\text{Expr}] & \text {prec} = 1 \\
        [\text{Expr}] + [\text{Expr}] & \text {prec} = 0 \\
        [\text{Expr}] - [\text{Expr}] & \text {prec} = 1 \\
        
    \end{cases}\\
    [\text{Term}] &\to
    \begin{cases}
        \text{intLit}\\
        \text{ident}
    \end{cases}
\end{align}

$$