int main(void) {
  int x = 1;
  int y = x ? (x + 2) : (x - 2);
  if (y > 2)
    return y;
  return 0;
}
