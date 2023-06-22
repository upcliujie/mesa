use std::ops::Add;
use std::ops::Div;
use std::ops::Rem;
use std::ops::Sub;

pub fn gcd<T>(mut a: T, mut b: T) -> T
where
    T: Copy + Default + PartialEq,
    T: Rem<Output = T>,
{
    let mut c = a % b;
    while c != T::default() {
        a = b;
        b = c;
        c = a % b;
    }

    b
}

#[test]
fn gcd_test() {
    assert_eq!(gcd(5, 15), 5);
    assert_eq!(gcd(7, 15), 1);
    assert_eq!(gcd(60, 45), 15);
}

pub fn align<T>(val: T, a: T) -> T
where
    T: Add<Output = T>,
    T: Copy,
    T: Default,
    T: PartialEq,
    T: Rem<Output = T>,
    T: Sub<Output = T>,
{
    let tmp = val % a;
    if tmp == T::default() {
        val
    } else {
        val + (a - tmp)
    }
}

pub fn div_round_up<T>(a: T, b: T) -> T
where
    T: Copy,
    T: Add<Output = T>,
    T: Div<Output = T>,
    T: Sub<Output = T>,
{
    #[allow(clippy::eq_op)]
    let one = b / b;

    (a + b - one) / b
}

pub struct SetBitIndices<T> {
    val: T,
}

impl<T> SetBitIndices<T> {
    pub fn from_msb(val: T) -> Self {
        Self { val: val }
    }
}

impl Iterator for SetBitIndices<u32> {
    type Item = u32;

    fn next(&mut self) -> Option<Self::Item> {
        if self.val == 0 {
            None
        } else {
            let pos = u32::BITS - self.val.leading_zeros() - 1;
            self.val ^= 1 << pos;
            Some(pos)
        }
    }
}

#[test]
fn align_test() {
    assert_eq!(align(0x1, 0x4), 0x4);
    assert_eq!(align(0x8, 0x2), 0x8);
    assert_eq!(align(0x7, 0x200), 0x200);
}
