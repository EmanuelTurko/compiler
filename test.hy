fun add(int a, int b) -> int {
def x = b / a;
ret x;
}

fun main() -> int
{
ret add(5,40);
}