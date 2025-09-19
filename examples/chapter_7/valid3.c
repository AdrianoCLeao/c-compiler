int main(void) {
  int a = 1;
  int b = 2;
  {
    int c = a ? b : 3;
    {
      int d = c + a;
      return d;
    }
  }
  return 0;
}
