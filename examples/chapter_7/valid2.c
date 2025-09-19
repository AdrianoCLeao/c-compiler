int main(void) {
  int a = 2;
  {
    int a = 5;
    if (a > 3)
      return a;
  }
  return a;
}
