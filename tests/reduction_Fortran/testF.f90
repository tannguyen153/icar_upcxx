subroutine accum(array, in_start, in_end, partialSum) bind(c, name="accum")
 use iso_c_binding, only : c_int
 integer(c_int), intent(in) :: array(in_start:in_end)
 integer(c_int), intent(in) :: in_start, in_end
 integer(c_int), intent(out) :: partialSum 
 integer(c_int) :: i,s
 s = 0
 do i=in_start,in_end
   s = s + array(i)
 end do
 partialSum = s
end subroutine accum 
