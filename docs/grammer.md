$$
\begin{align}
    [\text{Prog}] &\to [\text{Stmt}]^* \\[6pt]

    [\text{Stmt}] &\to
    \begin{cases}
        \text{exit}([\text{Expr}]); \\
        \text{let}\ \text{ident} = [\text{Expr}]; \\
        \text{if}([\text{Expr}])[\text{Scope}][\text{Elif}] \\
        [\text{Scope}]
    \end{cases} \\[6pt]

    [\text{Scope}] &\to \{[\text{Stmt}]^*\} \\[6pt]

    [\text{IfPred}] &\to
    \begin{cases}
        \text{elif}([\text{Expr}])[\text{Scope}][\text{IfPred}] \\
        \text{else}\text{[Scope]}\\
        \epsilon
    \end{cases} \\[6pt]

    [\text{Expr}] &\to
    \begin{cases}
        [\text{Term}] \\
        [\text{BinExpr}]
    \end{cases} \\[6pt]

    [\text{BinExpr}] &\to
    \begin{cases}
        [\text{Expr}] * [\text{Expr}] & \text{prec} = 1 \\
        [\text{Expr}] / [\text{Expr}] & \text{prec} = 1 \\
        [\text{Expr}] + [\text{Expr}] & \text{prec} = 0 \\
        [\text{Expr}] - [\text{Expr}] & \text{prec} = 0
    \end{cases} \\[6pt]

    [\text{Term}] &\to
    \begin{cases}
        \text{intLit} \\
        \text{ident} \\
        ([\text{Expr}])
    \end{cases}
\end{align}
$$