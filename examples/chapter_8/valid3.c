int main(void) {
  int sum = 0;
  for (int i = 0; i < 6; i = i + 1) {
    if (i == 3)
      continue;
    if (i > 4)
      break;
    sum = sum + i;
  }
  return sum;
}
